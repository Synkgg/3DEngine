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

Application::Application()
    : m_Running(false),
    m_Window("MyEngine", 1280, 720),
    m_Renderer(),
    m_Input(),
    m_Time(),
    m_ImGuiLayer(),
    m_Editor(),
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
    m_ProjectSettings.EnsureLoaded();
    m_Renderer.SetRenderSettings(m_ProjectSettings.GetRenderSettings());
    m_Runtime.SetProjectSettings(&m_ProjectSettings);

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




