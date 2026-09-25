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
#include "../Audio/AudioEngine.h"

#include "../Scene/Scene.h"

#include "../Scene/Runtime/Runtime.h"


class Application
{
public:
    Application();

    bool Initialize();
    void Run();
    void Shutdown();

    void StartRuntime();
    void StopRuntime();

private:

    void UpdateLighting();

    bool m_Running;
    Window m_Window;
    Renderer m_Renderer;
    Input m_Input;

    ImGuiLayer m_ImGuiLayer;
    Editor m_Editor;

    Time m_Time;
    AudioEngine m_Audio;
    ProjectSettings m_ProjectSettings;

    bool m_CameraControlActive;
    bool m_RuntimeMouseCaptured;

    Scene m_Scene;
    Runtime m_Runtime;

    UICanvas m_UICanvas;
    UIEditor m_UIEditor;
};