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

Application::Application(const std::string& projectPath)
    : m_Running(false),
    m_Window("MyEngine", 1280, 720),
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
    m_Audio.Initialize();
    m_Renderer.GetUIRenderer().SetAudioEngine(&m_Audio);
    m_Runtime.SetAudioEngine(&m_Audio);
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
        const std::string defaultLocation = (std::filesystem::current_path() / "Projects").string();
        std::snprintf(m_NewProjectLocation, sizeof(m_NewProjectLocation), "%s", defaultLocation.c_str());
    }
    const Project& activeProject = m_ProjectManager.GetActiveProject();
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






bool Application::ActivateProject(const std::string& descriptorPath)
{
    if (!m_ProjectManager.Load(descriptorPath))
    {
        m_ProjectHubError = "Could not open that project.";
        return false;
    }

    const Project& project = m_ProjectManager.GetActiveProject();
    if (!m_ProjectSettings.Load(project.GetSettingsPath().string()))
        m_ProjectSettings.Save(project.GetSettingsPath().string());

    m_Renderer.SetRenderSettings(m_ProjectSettings.GetRenderSettings());
    m_Runtime.SetProjectSettings(&m_ProjectSettings);
    m_Editor.ConfigureProject(project.GetAssetRoot(), project.GetSettingsPath());

    if (!project.startupScene.empty() &&
        !m_Editor.OpenScene(m_Scene, project.GetStartupScenePath()))
    {
        m_ProjectHubError = "Project opened, but its startup scene could not be loaded.";
        return false;
    }

    m_ProjectPath = descriptorPath;
    m_ShowProjectHub = false;
    m_ProjectHubError.clear();
    SDL_SetWindowTitle(m_Window.GetNativeWindow(), (project.name + " - Editor").c_str());
    return true;
}

bool Application::CreateProject(const std::string& parentDirectory, const std::string& name)
{
    if (name.empty() || parentDirectory.empty())
    {
        m_ProjectHubError = "Project name and location are required.";
        return false;
    }

    namespace fs = std::filesystem;
    const fs::path root = fs::path(parentDirectory) / name;
    std::error_code ec;
    fs::create_directories(root / "Assets" / "Scenes", ec);
    fs::create_directories(root / "Assets" / "Scripts", ec);
    fs::create_directories(root / "Assets" / "UI", ec);
    if (ec)
    {
        m_ProjectHubError = "Could not create the project folders.";
        return false;
    }

    const fs::path descriptor = root / (name + ".project");
    std::ofstream out(descriptor);
    if (!out)
    {
        m_ProjectHubError = "Could not create the project descriptor.";
        return false;
    }

    out << "Version 1\n"
        << "Name " << std::quoted(name) << "\n"
        << "AssetDirectory \"Assets\"\n"
        << "StartupScene \"\"\n"
        << "Settings \"ProjectSettings.cfg\"\n";
    out.close();

    ProjectSettings defaults;
    if (!defaults.Save((root / "ProjectSettings.cfg").string()))
    {
        m_ProjectHubError = "Could not create project settings.";
        return false;
    }

    return ActivateProject(descriptor.string());
}

void Application::RenderProjectHub()
{
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::Begin("Project Hub", nullptr, flags);
    const float width = ImGui::GetContentRegionAvail().x;
    ImGui::Dummy(ImVec2(0, 42));
    ImGui::SetCursorPosX((width - 520.0f) * 0.5f);
    ImGui::BeginChild("HubCard", ImVec2(520, 0), ImGuiChildFlags_Borders);

    ImGui::Dummy(ImVec2(0, 20));
    ImGui::SetWindowFontScale(1.45f);
    ImGui::TextUnformatted("Projects");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::TextDisabled("Create a game project or open an existing one.");
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("Open Project...", ImVec2(-1, 42)))
    {
        std::string path;
        if (FileDialog::OpenProject(path))
            ActivateProject(path);
    }

    ImGui::Spacing();
    ImGui::TextUnformatted("New Project");
    ImGui::InputText("Name", m_NewProjectName, sizeof(m_NewProjectName));
    ImGui::InputText("Location", m_NewProjectLocation, sizeof(m_NewProjectLocation));
    ImGui::SameLine();
    if (ImGui::Button("Browse..."))
    {
        std::string folder;
        if (FileDialog::SelectFolder(folder))
            std::snprintf(m_NewProjectLocation, sizeof(m_NewProjectLocation), "%s", folder.c_str());
    }

    if (ImGui::Button("Create Project", ImVec2(-1, 42)))
        CreateProject(m_NewProjectLocation, m_NewProjectName);

    ImGui::Spacing();
    ImGui::Separator();
    if (ImGui::Button("Continue Legacy Workspace", ImVec2(-1, 34)))
    {
        m_ShowProjectHub = false;
        SDL_SetWindowTitle(m_Window.GetNativeWindow(), "Editor - Legacy Workspace");
    }

    if (!m_ProjectHubError.empty())
    {
        ImGui::Spacing();
        ImGui::TextWrapped("%s", m_ProjectHubError.c_str());
    }

    ImGui::EndChild();
    ImGui::End();
}
