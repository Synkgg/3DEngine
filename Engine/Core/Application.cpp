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
    if (!SDL_Init(SDL_INIT_VIDEO))
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

    UITest::Run();

    // Runtime UI is opt-in. Scenes/scripts explicitly load the UI they need
    // through UI.Load(), rather than inheriting whichever asset was open in
    // the Widget Blueprint editor.
    m_UICanvas.Clear();

    return true;
}

void Application::Run()
{
    m_Running = true;

    SDL_Event event;

    while (m_Running)
    {
        m_Time.Update();

        while (SDL_PollEvent(&event))
        {
            // While gameplay owns relative mouse input, do not feed mouse
            // motion/buttons/wheel into Dear ImGui. SDL relative mode hides
            // the OS cursor, but ImGui otherwise keeps integrating those
            // events into its own virtual mouse position.
            const bool runtimeOwnsMouse =
                m_Runtime.IsRunning() &&
                m_RuntimeMouseCaptured;

            const bool mouseEvent =
                event.type == SDL_EVENT_MOUSE_MOTION ||
                event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
                event.type == SDL_EVENT_MOUSE_BUTTON_UP ||
                event.type == SDL_EVENT_MOUSE_WHEEL;

            if (!(runtimeOwnsMouse && mouseEvent))
            {
                m_ImGuiLayer.ProcessEvent(&event);
            }

            if (event.type == SDL_EVENT_QUIT)
            {
                m_Running = false;
            }

            if (event.type ==
                SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
            {
                m_Window.UpdateSize();

                int pixelWidth = 0;
                int pixelHeight = 0;

                SDL_GetWindowSizeInPixels(
                    m_Window.GetNativeWindow(),
                    &pixelWidth,
                    &pixelHeight
                );

                if (pixelWidth > 0 &&
                    pixelHeight > 0)
                {
                    m_Renderer.ResizeViewport(
                        pixelWidth,
                        pixelHeight
                    );
                }
            }

            if (event.type == SDL_EVENT_KEY_DOWN &&
                event.key.scancode == SDL_SCANCODE_F11)
            {
                m_Window.ToggleFullscreen();
            }
        }

        m_Input.Update();

        // Escape is the editor-level emergency stop for Play mode. Runtime
        // pause menus use the same key first, but a second Escape while paused
        // stops Play mode so the editor is never trapped in runtime.
        if (m_Runtime.IsRunning() &&
            m_Runtime.IsPaused() &&
            m_Input.IsKeyPressed(SDL_SCANCODE_ESCAPE))
        {
            m_Editor.StopPlaying();
        }

        m_ImGuiLayer.BeginFrame();

        // ImGui's SDL backend can still query the mouse every frame even when
        // motion events are filtered. Disable mouse interaction completely
        // while the game owns the cursor, then restore it for menus/editor.
        ImGuiIO& imguiIO = ImGui::GetIO();
        if (m_Runtime.IsRunning() && m_RuntimeMouseCaptured)
        {
            imguiIO.ConfigFlags |= ImGuiConfigFlags_NoMouse;
            imguiIO.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
        }
        else
        {
            imguiIO.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
        }

        // The runtime owns the shared UI canvas while playing. Drawing the
        // widget editor against that same tree risks stale selections and
        // lets editor input compete with game UI.
        if (!m_Runtime.IsRunning())
        {
            m_UIEditor.Draw(m_UICanvas, m_Renderer);
        }

        m_Editor.Render(m_Renderer, m_Scene, m_ImGuiLayer.GetIconFont());

        // Never let editor asset-open requests replace the live runtime
        // canvas. Content-browser clicks can otherwise destroy widgets while
        // Lua still holds references to them.
        if (!m_Runtime.IsRunning())
        {
            const std::string openedUIAsset = m_Editor.ConsumeOpenedUIAsset();
            if (!openedUIAsset.empty())
            {
                if (!m_UIEditor.OpenAsset(m_UICanvas, openedUIAsset))
                {
                    Logger::Error(
                        std::string("Failed to open UI asset: ") +
                        openedUIAsset);
                }
            }
        }

        if (m_Editor.IsPlaying() &&
            !m_Runtime.IsRunning())
        {
            StartRuntime();
        }
        else if (!m_Editor.IsPlaying() &&
            m_Runtime.IsRunning())
        {
            StopRuntime();
        }

        if (!m_Runtime.IsRunning())
        {
            bool rightMouseDown =
                m_Input.IsMouseButtonDown(
                    SDL_BUTTON_RIGHT
                );

            bool captureStarted = false;

            if (!m_CameraControlActive)
            {
                if (m_Editor.IsViewportHovered() &&
                    rightMouseDown)
                {
                    m_CameraControlActive = true;
                    captureStarted = true;

                    m_Input.SetMouseCapture(
                        m_Window.GetNativeWindow(),
                        true
                    );
                }
            }
            else if (!rightMouseDown)
            {
                m_CameraControlActive = false;

                m_Input.SetMouseCapture(
                    m_Window.GetNativeWindow(),
                    false
                );
            }

            if (captureStarted)
            {
                m_Input.Update();
            }

            if (m_CameraControlActive)
            {
                constexpr float mouseSensitivity =
                    0.003f;

                m_Renderer.RotateCamera(
                    m_Input.GetMouseDeltaX() *
                    mouseSensitivity,

                    -m_Input.GetMouseDeltaY() *
                    mouseSensitivity
                );

                float forward = 0.0f;
                float right = 0.0f;
                float up = 0.0f;

                if (m_Input.IsKeyDown(
                    SDL_SCANCODE_W))
                {
                    forward += 1.0f;
                }

                if (m_Input.IsKeyDown(
                    SDL_SCANCODE_S))
                {
                    forward -= 1.0f;
                }

                if (m_Input.IsKeyDown(
                    SDL_SCANCODE_D))
                {
                    right += 1.0f;
                }

                if (m_Input.IsKeyDown(
                    SDL_SCANCODE_A))
                {
                    right -= 1.0f;
                }

                if (m_Input.IsKeyDown(
                    SDL_SCANCODE_E))
                {
                    up += 1.0f;
                }

                if (m_Input.IsKeyDown(
                    SDL_SCANCODE_Q))
                {
                    up -= 1.0f;
                }

                m_Renderer.MoveCamera(
                    forward,
                    right,
                    up,
                    m_Time.GetDeltaTime()
                );
            }
        }
        else
        {
            if (m_CameraControlActive)
            {
                m_CameraControlActive = false;

                m_Input.SetMouseCapture(
                    m_Window.GetNativeWindow(),
                    false
                );
            }
        }

        if (m_Runtime.IsRunning())
        {
            const bool gameplayScene = !m_Runtime.WantsCursor();

            // Menu scenes keep the cursor free for canvas buttons.
            // Gameplay captures automatically after the menu has switched
            // scenes, so the Play click itself can never steal the mouse.
            if (gameplayScene && !m_RuntimeMouseCaptured)
            {
                m_Input.SetMouseCapture(
                    m_Window.GetNativeWindow(),
                    true
                );
                m_RuntimeMouseCaptured = true;
                m_Input.Update();
            }
            else if (!gameplayScene && m_RuntimeMouseCaptured)
            {
                m_Input.SetMouseCapture(
                    m_Window.GetNativeWindow(),
                    false
                );
                m_RuntimeMouseCaptured = false;
            }
        }
        else if (m_RuntimeMouseCaptured)
        {
            m_Input.SetMouseCapture(
                m_Window.GetNativeWindow(),
                false
            );
            m_RuntimeMouseCaptured = false;
        }

        // Update UI input before Lua. UI.WasClicked() consumes the click
        // during the runtime update, so hit testing must happen first.
        if (m_Runtime.IsRunning())
        {
            UIRenderer& ui = m_Renderer.GetUIRenderer();
            const Vec2 canvasSize = m_UICanvas.GetSize();
            const ImVec2 gameViewportPosition = m_Editor.GetViewportPosition();
            const ImVec2 gameViewportSize = m_Editor.GetViewportSize();

            ui.SetLogicalSize(canvasSize.x, canvasSize.y);
            ui.UpdateInput(
                m_UICanvas,
                m_Input,
                gameViewportPosition.x,
                gameViewportPosition.y,
                gameViewportSize.x,
                gameViewportSize.y
            );

            m_Runtime.Update(
                m_Scene,
                m_Renderer,
                m_Input,
                m_Time.GetDeltaTime()
            );
        }

        UpdateLighting();

        m_Renderer.BeginFrame();

        m_Renderer.DrawSky();

        if (!m_Runtime.IsRunning())
        {
            m_Renderer.DrawGrid();
        }

        for (const Entity& entity : m_Scene.GetEntities())
        {
            TransformComponent* transform =
                m_Scene.GetComponent<TransformComponent>(
                    entity
                );

            MeshComponent* mesh =
                m_Scene.GetComponent<MeshComponent>(
                    entity
                );

            ColorComponent* color =
                m_Scene.GetComponent<ColorComponent>(
                    entity
                );

            TextureComponent* textureComponent =
                m_Scene.GetComponent<TextureComponent>(
                    entity
                );

            if (transform != nullptr &&
                mesh != nullptr)
            {
                float red = 1.0f;
                float green = 1.0f;
                float blue = 1.0f;
                float alpha = 1.0f;

                if (color != nullptr)
                {
                    red = color->r;
                    green = color->g;
                    blue = color->b;
                    alpha = color->a;
                }

                Texture2D* texture = nullptr;

                if (textureComponent != nullptr &&
                    !textureComponent->path.empty())
                {
                    texture =
                        m_Renderer.LoadTexture(
                            textureComponent->path
                        );
                }

                Transform meshTransform =
                    transform->transform;

                meshTransform.position.x +=
                    mesh->offset.x;

                meshTransform.position.y +=
                    mesh->offset.y;

                meshTransform.position.z +=
                    mesh->offset.z;

                meshTransform.rotation.x +=
                    mesh->rotation.x;

                meshTransform.rotation.y +=
                    mesh->rotation.y;

                meshTransform.rotation.z +=
                    mesh->rotation.z;

                MaterialComponent* material =
                    m_Scene.GetComponent<MaterialComponent>(entity);

                m_Renderer.DrawMesh(
                    meshTransform,
                    mesh->primitive,
                    red,
                    green,
                    blue,
                    alpha,
                    texture,
                    material ? material->metallic : 0.0f,
                    material ? material->roughness : 0.65f,
                    material ? material->ambientOcclusion : 1.0f,
                    material ? material->emissive : 0.0f
                );
            }
        }

        Entity selectedEntity =
            m_Editor.GetSelectedEntity();

        LightComponent* selectedLight = nullptr;

        TransformComponent* selectedTransform = nullptr;

        if (selectedEntity.IsValid())
        {
            selectedLight =
                m_Scene.GetComponent<LightComponent>(
                    selectedEntity
                );

            selectedTransform =
                m_Scene.GetComponent<TransformComponent>(
                    selectedEntity
                );
        }

        if (selectedLight != nullptr &&
            selectedTransform != nullptr)
        {
            m_Renderer.DrawDirectionalLight(
                selectedTransform->transform.position,
                selectedLight->direction
            );
        }

        if (selectedEntity.IsValid())
        {
            ColliderComponent* collider =
                m_Scene.GetComponent<
                ColliderComponent
                >(selectedEntity);

            TransformComponent* transform =
                m_Scene.GetComponent<
                TransformComponent
                >(selectedEntity);

            if (collider != nullptr &&
                collider->enabled &&
                transform != nullptr)
            {
                const Vec3 scale =
                    transform->transform.scale;

                m_Renderer.DrawCollider(
                    transform->transform,
                    collider->width *
                    std::abs(scale.x),

                    collider->height *
                    std::abs(scale.y),

                    collider->depth *
                    std::abs(scale.z)
                );
            }
        }

        /*
         * Game UI
         */
        if (m_Runtime.IsRunning())
        {
            UIRenderer& ui =
                m_Renderer.GetUIRenderer();

            const Vec2 canvasSize =
                m_UICanvas.GetSize();

            ui.SetLogicalSize(
                canvasSize.x,
                canvasSize.y
            );

            ui.Begin();

            ui.RenderCanvas(
                m_UICanvas,
                &m_Renderer
            );

            ui.End();
        }

        m_Renderer.EndScene();

        m_ImGuiLayer.EndFrame();

        m_Renderer.EndFrame();
    }
}

void Application::Shutdown()
{
    m_ImGuiLayer.Shutdown();
    m_Renderer.Shutdown();
    m_Window.Shutdown();

    SDL_Quit();
}

void Application::StartRuntime()
{
    m_Renderer.GetUIRenderer().Clear();

    // Runtime UI starts empty every time. Lua decides which UI asset is active.
    // This prevents an editor-opened UI from leaking into every scene.
    m_UICanvas.Clear();

    m_Runtime.SaveCameraState(m_Renderer);

    m_Runtime.Start(
        m_Scene,
        m_Renderer,
        m_Input,
        m_UICanvas
    );
}

void Application::StopRuntime()
{
    m_Renderer.GetUIRenderer().Clear();

    m_Runtime.Stop(
        m_Scene
    );

    m_Runtime.RestoreCameraState(
        m_Renderer
    );
}

void Application::UpdateLighting()
{
    LightComponent* directional = nullptr;
    m_Renderer.ClearLocalLights();

    for (const Entity& entity : m_Scene.GetEntities())
    {
        LightComponent* light = m_Scene.GetComponent<LightComponent>(entity);
        TransformComponent* transform = m_Scene.GetComponent<TransformComponent>(entity);
        if (!light) continue;

        if (light->type == LightType::Directional && directional == nullptr)
        {
            directional = light;
        }
        else if (light->type == LightType::Point && transform)
        {
            PointLightData data;
            data.position = transform->transform.position;
            data.color = light->color;
            data.intensity = light->intensity;
            data.range = light->range;
            m_Renderer.AddPointLight(data);
        }
        else if (light->type == LightType::Spot && transform)
        {
            SpotLightData data;
            data.position = transform->transform.position;
            data.direction = light->direction;
            data.color = light->color;
            data.intensity = light->intensity;
            data.range = light->range;
            data.innerCos = std::cos(light->innerAngle * 0.0174532925f);
            data.outerCos = std::cos(light->outerAngle * 0.0174532925f);
            m_Renderer.AddSpotLight(data);
        }
    }

    if (directional)
        m_Renderer.SetDirectionalLight(directional->direction, directional->color, directional->intensity);
    else
        m_Renderer.SetDirectionalLight(Vec3(-0.5f,-1.0f,-0.5f), Vec3(1.0f,0.95f,0.88f), 1.0f);
}
