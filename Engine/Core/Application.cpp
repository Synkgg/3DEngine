#include "Application.h"
#include <SDL3/SDL.h>
#include <iostream>
#include <cfloat>
#include <imgui.h>

#include "../Scene/Entity.h"

#include "../Scene/Components/TransformComponent.h"
#include "../Scene/Components/MeshComponent.h"
#include "../Scene/Components/ColorComponent.h"
#include "../Scene/Components/LightComponent.h"
#include "../Scene/Components/ColliderComponent.h"
#include "../Scene/Components/TextureComponent.h"
#include "../Scene/Components/MaterialComponent.h"

#include "../Graphics/PrimitiveType.h"

#include "../Core/Logger.h"

#include "../UI/UITest.h"
#include "../UI/UISerializer.h"
#include "../UI/UIText.h"
#include "../Platform/Windows/FileDialog.h"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <cstdio>


Application::Application(const std::string& projectPath)
    : m_Running(false),
    m_Window("Velcryn Hub", 1280, 720),
    m_Renderer(),
    m_Input(),
    m_Time(),
    m_ImGuiLayer(),
    m_Editor(),
    m_ProjectPath(projectPath),
    m_CameraControlActive(false),
    m_RuntimeMouseCaptured(false)
{
}

bool Application::Initialize()
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO))
    {
        Logger::Error( std::string("Failed to initialize SDL: ") + SDL_GetError());

        m_Running = false;
        return false;
    }

    if (!m_Window.Initialize())
    {
        SDL_Quit();
        m_Running = false;
        return false;
    }

    if (!m_Renderer.Initialize(m_Window))
    {
        m_Window.Shutdown();
        SDL_Quit();
        m_Running = false;
        return false;
    }

    if (!m_ImGuiLayer.Initialize(
        m_Window,
        m_Renderer.GetContext()))
    {
        m_Renderer.Shutdown();
        m_Window.Shutdown();
        SDL_Quit();
        m_Running = false;
        return false;
    }

#ifdef ENGINE_DEBUG
    UITest::Run();
#endif

    // Runtime UI is opt-in. Scenes/scripts explicitly load the UI they need
    // through UI.Load(), rather than inheriting whichever asset was open in
    // the Widget Blueprint editor.
    m_UICanvas.Clear();
    m_UIDocuments.clear();
    m_ActiveUIDocument = -1;
    m_Audio.Initialize();
    m_Renderer.GetUIRenderer().SetAudioEngine(&m_Audio);
    m_Runtime.SetAudioEngine(&m_Audio);
    m_Runtime.SetProjectManager(&m_ProjectManager);
    m_ProjectHub.Initialize();
    if (!m_ProjectPath.empty())
    {
        if (!m_ProjectManager.Load(m_ProjectPath)) return false;
        if (!m_ProjectSettings.Load(m_ProjectManager.GetActiveProject().GetSettingsPath().string()))
            m_ProjectSettings.Save(m_ProjectManager.GetActiveProject().GetSettingsPath().string());
    }
    else
    {
        m_ProjectManager.UseLegacyWorkspace();
        m_ProjectSettings.EnsureLoaded();
        m_ShowProjectHub = true;
    }
    const Project& activeProject = m_ProjectManager.GetActiveProject();

    // Keep asset references portable without changing the process working
    // directory. Renderer/runtime resolve authored Assets/... paths against
    // the active project explicitly.
    m_Renderer.SetProjectRoot(activeProject.rootDirectory);
    m_Audio.SetProjectRoot(activeProject.rootDirectory);
    m_Editor.ConfigureProject(activeProject.GetAssetRoot(), activeProject.GetSettingsPath());

    // The editor owns the live project settings instance. Runtime and renderer
    // must share that same object so edits cannot diverge from Play mode.
    ProjectSettings& liveProjectSettings = m_Editor.GetProjectSettings();
    m_Renderer.SetRenderSettings(liveProjectSettings.GetRenderSettings());
    m_Runtime.SetProjectSettings(&liveProjectSettings);

    if (m_ProjectManager.HasProject() && !activeProject.startupScene.empty())
    {
        if (!m_Editor.OpenScene(m_Scene, activeProject.GetStartupScenePath()))
            return false;
    }

    if (m_ProjectManager.HasProject())
    {
        m_ProjectHub.AddRecentProject(activeProject);
        SDL_SetWindowTitle(m_Window.GetNativeWindow(), (activeProject.name + " - Velcryn Editor").c_str());
    }

    return true;
}


void Application::Shutdown()
{
    // Runtime owns Lua scripts and network state that can still reference the
    // UI canvas, renderer, input, and audio systems. Stop it while all of
    // those dependencies are still alive. Previously the application could
    // tear down SDL/audio/rendering first and leave runtime/UI pointers alive
    // until object destruction, causing a shutdown-time read access violation.
    if (m_Runtime.IsRunning())
    {
        m_Editor.StopPlaying();
        StopRuntime();
    }

    // UIRenderer keeps a non-owning AudioEngine pointer for button sounds.
    // Detach it before the audio engine is shut down so no late UI cleanup can
    // observe a destroyed audio backend.
    m_Renderer.GetUIRenderer().SetMouseInteractionEnabled(false);
    m_Renderer.GetUIRenderer().SetAudioEngine(nullptr);
    m_Renderer.GetUIRenderer().Clear();
    m_UICanvas.Clear();

    m_Audio.Shutdown();
    m_ImGuiLayer.Shutdown();
    m_Renderer.Shutdown();
    m_Window.Shutdown();

    SDL_Quit();
}






void Application::ReturnToProjectHub()
{
    // Leave the active workspace in a predictable state before showing the Hub.
    if (m_Runtime.IsRunning())
    {
        m_Editor.StopPlaying();
        StopRuntime();
    }

    if (m_CameraControlActive || m_RuntimeMouseCaptured)
    {
        m_Input.SetMouseCapture(m_Window.GetNativeWindow(), false);
        m_CameraControlActive = false;
        m_RuntimeMouseCaptured = false;
    }

    m_Renderer.GetUIRenderer().SetMouseInteractionEnabled(false);
    m_Renderer.GetUIRenderer().Clear();
    m_UICanvas.Clear();
    m_UIDocuments.clear();
    m_ActiveUIDocument = -1;

    // The Hub owns no scene. Drop project entities/resources from the editor
    // workspace, then use the compatibility workspace until another project
    // is selected.
    m_Scene = Scene();
    m_ProjectManager.UseLegacyWorkspace();
    m_ProjectPath.clear();

    const Project& workspace = m_ProjectManager.GetActiveProject();
    m_Renderer.SetProjectRoot(workspace.rootDirectory);
    m_Audio.SetProjectRoot(workspace.rootDirectory);
    m_Editor.ConfigureProject(workspace.GetAssetRoot(), workspace.GetSettingsPath());

    ProjectSettings& liveSettings = m_Editor.GetProjectSettings();
    m_Renderer.SetRenderSettings(liveSettings.GetRenderSettings());
    m_Runtime.SetProjectSettings(&liveSettings);
    m_Runtime.SetProjectManager(&m_ProjectManager);

    m_ShowProjectHub = true;
    m_ProjectHub.ClearError();
    SDL_SetWindowTitle(m_Window.GetNativeWindow(), "Velcryn Hub");
    Logger::Info("Returned to Velcryn Hub.");
}

bool Application::ActivateProject(const std::string& descriptorPath)
{
    if (!m_ProjectManager.Load(descriptorPath))
    {
        m_ProjectHub.SetError("Could not open that project.");
        return false;
    }

    const Project& project = m_ProjectManager.GetActiveProject();

    m_Renderer.SetProjectRoot(project.rootDirectory);
    m_Audio.SetProjectRoot(project.rootDirectory);
    m_Editor.ConfigureProject(project.GetAssetRoot(), project.GetSettingsPath());
    ProjectSettings& liveProjectSettings = m_Editor.GetProjectSettings();
    m_Renderer.SetRenderSettings(liveProjectSettings.GetRenderSettings());
    m_Runtime.SetProjectSettings(&liveProjectSettings);
    m_Runtime.SetProjectManager(&m_ProjectManager);

    if (!project.startupScene.empty() &&
        !m_Editor.OpenScene(m_Scene, project.GetStartupScenePath()))
    {
        m_ProjectHub.SetError("Project opened, but its startup scene could not be loaded.");
        return false;
    }

    m_ProjectPath = project.descriptorPath.string();
    m_ProjectHub.AddRecentProject(project);
    m_ShowProjectHub = false;
    m_ProjectHub.ClearError();
    SDL_SetWindowTitle(m_Window.GetNativeWindow(), (project.name + " - Velcryn Editor").c_str());
    return true;
}

bool Application::CreateProject(const std::string& parentDirectory, const std::string& name)
{
    if (name.empty() || parentDirectory.empty())
    {
        m_ProjectHub.SetError("Project name and location are required.");
        return false;
    }

    const std::filesystem::path root =
        (std::filesystem::path(parentDirectory) / name).lexically_normal();

    if (!m_ProjectManager.Create(root.string(), name))
    {
        m_ProjectHub.SetError("Could not create the project workspace.");
        return false;
    }

    return ActivateProject(
        m_ProjectManager.GetActiveProject().descriptorPath.string());
}













