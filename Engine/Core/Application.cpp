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
    m_Runtime.SetProjectManager(&m_ProjectManager);
    LoadRecentProjects();
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

    // Keep asset references portable without changing the process working
    // directory. Renderer/runtime resolve authored Assets/... paths against
    // the active project explicitly.
    m_Renderer.SetProjectRoot(activeProject.rootDirectory);
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
        AddRecentProject(activeProject);
        SDL_SetWindowTitle(m_Window.GetNativeWindow(), (activeProject.name + " - Editor").c_str());
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

    m_Renderer.SetProjectRoot(project.rootDirectory);
    m_Editor.ConfigureProject(project.GetAssetRoot(), project.GetSettingsPath());
    ProjectSettings& liveProjectSettings = m_Editor.GetProjectSettings();
    m_Renderer.SetRenderSettings(liveProjectSettings.GetRenderSettings());
    m_Runtime.SetProjectSettings(&liveProjectSettings);
    m_Runtime.SetProjectManager(&m_ProjectManager);

    if (!project.startupScene.empty() &&
        !m_Editor.OpenScene(m_Scene, project.GetStartupScenePath()))
    {
        m_ProjectHubError = "Project opened, but its startup scene could not be loaded.";
        return false;
    }

    m_ProjectPath = project.descriptorPath.string();
    AddRecentProject(project);
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

    const std::filesystem::path root =
        (std::filesystem::path(parentDirectory) / name).lexically_normal();

    if (!m_ProjectManager.Create(root.string(), name))
    {
        m_ProjectHubError = "Could not create the project workspace.";
        return false;
    }

    return ActivateProject(
        m_ProjectManager.GetActiveProject().descriptorPath.string());
}

std::string Application::GetHubStatePath() const
{
    char* prefPath = SDL_GetPrefPath("3DEngine", "Editor");
    if (prefPath == nullptr)
        return (std::filesystem::current_path() / "Saved" / "RecentProjects.txt").string();

    std::filesystem::path path(prefPath);
    SDL_free(prefPath);
    return (path / "RecentProjects.txt").string();
}

void Application::LoadRecentProjects()
{
    m_RecentProjects.clear();

    std::ifstream in(GetHubStatePath());
    if (!in)
        return;

    std::string name;
    std::string path;
    while (in >> std::quoted(name) >> std::quoted(path))
    {
        if (!path.empty())
            m_RecentProjects.push_back({ name, path });
    }
}

void Application::SaveRecentProjects() const
{
    const std::filesystem::path statePath(GetHubStatePath());
    std::error_code error;
    std::filesystem::create_directories(statePath.parent_path(), error);
    if (error)
        return;

    std::ofstream out(statePath, std::ios::trunc);
    if (!out)
        return;

    for (const RecentProject& recent : m_RecentProjects)
        out << std::quoted(recent.name) << ' ' << std::quoted(recent.descriptorPath) << '\n';
}

void Application::AddRecentProject(const Project& project)
{
    if (project.descriptorPath.empty())
        return;

    const std::string path =
        std::filesystem::absolute(project.descriptorPath).lexically_normal().string();

    m_RecentProjects.erase(
        std::remove_if(
            m_RecentProjects.begin(),
            m_RecentProjects.end(),
            [&](const RecentProject& recent)
            {
                return std::filesystem::path(recent.descriptorPath).lexically_normal() ==
                       std::filesystem::path(path).lexically_normal();
            }),
        m_RecentProjects.end());

    m_RecentProjects.insert(m_RecentProjects.begin(), { project.name, path });
    if (m_RecentProjects.size() > 12)
        m_RecentProjects.resize(12);

    SaveRecentProjects();
}

void Application::RemoveRecentProject(std::size_t index)
{
    if (index >= m_RecentProjects.size())
        return;

    m_RecentProjects.erase(m_RecentProjects.begin() + static_cast<std::ptrdiff_t>(index));
    SaveRecentProjects();
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

    const float panelWidth = 760.0f;
    const float availableWidth = ImGui::GetContentRegionAvail().x;
    ImGui::Dummy(ImVec2(0, 28));
    ImGui::SetCursorPosX(std::max(16.0f, (availableWidth - panelWidth) * 0.5f));
    ImGui::BeginChild("HubCard", ImVec2(std::min(panelWidth, availableWidth - 32.0f), 0), ImGuiChildFlags_Borders);

    ImGui::Dummy(ImVec2(0, 18));
    ImGui::SetWindowFontScale(1.55f);
    ImGui::TextUnformatted("Projects");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::TextDisabled("Create, open, and return to your game projects.");
    ImGui::Spacing();

    if (ImGui::Button("Open Project...", ImVec2(180, 40)))
    {
        std::string path;
        if (FileDialog::OpenProject(path))
            ActivateProject(path);
    }

    ImGui::SameLine();
    if (ImGui::Button("Refresh Recents", ImVec2(150, 40)))
        LoadRecentProjects();

    ImGui::Separator();
    ImGui::TextUnformatted("Recent Projects");

    if (m_RecentProjects.empty())
    {
        ImGui::TextDisabled("No recent projects yet.");
    }
    else
    {
        std::size_t removeIndex = static_cast<std::size_t>(-1);

        for (std::size_t i = 0; i < m_RecentProjects.size(); ++i)
        {
            const RecentProject& recent = m_RecentProjects[i];
            const bool exists = std::filesystem::is_regular_file(recent.descriptorPath);

            ImGui::PushID(static_cast<int>(i));
            ImGui::BeginGroup();
            ImGui::TextUnformatted(recent.name.c_str());
            ImGui::TextDisabled("%s", recent.descriptorPath.c_str());
            ImGui::EndGroup();

            const float buttonsWidth = 190.0f;
            ImGui::SameLine(std::max(300.0f, ImGui::GetContentRegionAvail().x - buttonsWidth));

            ImGui::BeginDisabled(!exists);
            if (ImGui::Button("Open", ImVec2(82, 32)))
                ActivateProject(recent.descriptorPath);
            ImGui::EndDisabled();

            ImGui::SameLine();
            if (ImGui::Button("Remove", ImVec2(82, 32)))
                removeIndex = i;

            if (!exists)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("(missing)");
            }

            ImGui::Separator();
            ImGui::PopID();
        }

        if (removeIndex != static_cast<std::size_t>(-1))
            RemoveRecentProject(removeIndex);
    }

    ImGui::Spacing();
    ImGui::TextUnformatted("New Project");
    ImGui::InputText("Project Name", m_NewProjectName, sizeof(m_NewProjectName));
    ImGui::InputText("Location", m_NewProjectLocation, sizeof(m_NewProjectLocation));

    ImGui::SameLine();
    if (ImGui::Button("Browse..."))
    {
        std::string folder;
        if (FileDialog::SelectFolder(folder))
            std::snprintf(m_NewProjectLocation, sizeof(m_NewProjectLocation), "%s", folder.c_str());
    }

    std::filesystem::path preview =
        (std::filesystem::path(m_NewProjectLocation) / m_NewProjectName).lexically_normal();
    ImGui::TextDisabled("Project folder: %s", preview.string().c_str());

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
