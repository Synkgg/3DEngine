#pragma once

#include "../Platform/SDL/Window.h"
#include "../Graphics/Renderer.h"
#include "../Platform/SDL/Input.h"

#include "../Editor/ImGuiLayer.h"
#include "../Editor/Editor.h"
#include "../Editor/UI/UIEditor.h"

#include "../UI/UICanvas.h"

#include "Time.h"
#include "ProjectSettings.h"
#include "ProjectManager.h"
#include "../Audio/AudioEngine.h"

#include "../Scene/Scene.h"

#include "../Scene/Runtime/Runtime.h"
#include <vector>


class Application
{
public:
    explicit Application(const std::string& projectPath = {});

    bool Initialize();
    void Run();
    void Shutdown();

    void StartRuntime();
    bool ActivateProject(const std::string& descriptorPath);
    bool CreateProject(const std::string& parentDirectory, const std::string& name);
    void RenderProjectHub();
    void ReturnToProjectHub();
    void StopRuntime();

private:
    struct RecentProject
    {
        std::string name;
        std::string descriptorPath;
    };

    void UpdateLighting();
    void LoadRecentProjects();
    void SaveRecentProjects() const;
    void AddRecentProject(const Project& project);
    void RemoveRecentProject(std::size_t index);
    std::string GetHubStatePath() const;

    bool m_Running;
    Window m_Window;
    Renderer m_Renderer;
    Input m_Input;

    ImGuiLayer m_ImGuiLayer;
    Editor m_Editor;

    Time m_Time;
    AudioEngine m_Audio;
    ProjectManager m_ProjectManager;
    ProjectSettings m_ProjectSettings;

    std::string m_ProjectPath;
    bool m_ShowProjectHub = false;
    char m_NewProjectName[128]{ "New Project" };
    char m_NewProjectLocation[512]{};
    std::string m_ProjectHubError;
    std::vector<RecentProject> m_RecentProjects;

    bool m_CameraControlActive;
    bool m_RuntimeMouseCaptured;

    Scene m_Scene;
    Runtime m_Runtime;

    UICanvas m_UICanvas;
    UICanvas m_UIEditorCanvas;
    UIEditor m_UIEditor;
    bool m_UIEditorPageActive = false;
};