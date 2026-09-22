#include "Application.h"
#include <SDL3/SDL.h>
#include <iostream>

#include "../Scene/Entity.h"

#include "../Scene/Components/TransformComponent.h"
#include "../Scene/Components/MeshComponent.h"
#include "../Scene/Components/ColorComponent.h"
#include "../Scene/Components/LightComponent.h"
#include "../Scene/Components/ColliderComponent.h"
#include "../Scene/Components/TextureComponent.h"

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
            m_ImGuiLayer.ProcessEvent(&event);

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

        if (m_Runtime.IsRunning() &&
            m_Input.IsKeyDown(SDL_SCANCODE_ESCAPE))
        {
            m_Editor.StopPlaying();
        }

        m_ImGuiLayer.BeginFrame();

        m_UIEditor.Draw(m_UICanvas, m_Renderer);

        m_Editor.Render(m_Renderer, m_Scene, m_ImGuiLayer.GetIconFont());

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
            /*
             * Play-in-editor must leave the OS cursor available.
             * The new canvas UI uses absolute mouse coordinates for
             * hit testing, so relative mouse capture would make menus
             * impossible to click. Camera/player look can opt into
             * capture later when the game explicitly requests it.
             */
            if (m_RuntimeMouseCaptured)
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

            m_RuntimeMouseCaptured =
                false;
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

        if (m_Runtime.IsRunning())
        {
            const std::string& prompt =
                m_Runtime.GetInteractionPrompt();

            if (!prompt.empty())
            {
                const ImVec2 viewportPosition =
                    m_Editor.GetViewportPosition();

                const ImVec2 viewportSize =
                    m_Editor.GetViewportSize();

                ImDrawList* drawList =
                    ImGui::GetForegroundDrawList();

                /*
                 * Exact center of the game viewport.
                 * This is the same center used by the
                 * crosshair below.
                 */
                const float centerX =
                    viewportPosition.x +
                    viewportSize.x * 0.5f;

                const float centerY =
                    viewportPosition.y +
                    viewportSize.y * 0.5f;

                /*
                 * Place the interaction prompt directly
                 * below the center crosshair.
                 */
                const float promptCenterY =
                    centerY + 40.0f;

                const std::string keyText = "E";
                const std::string actionText = prompt;

                const float keySize = 32.0f;
                const float padding = 10.0f;
                const float spacing = 8.0f;

                const ImVec2 keyTextSize =
                    ImGui::CalcTextSize(
                        keyText.c_str()
                    );

                const ImVec2 actionTextSize =
                    ImGui::CalcTextSize(
                        actionText.c_str()
                    );

                const float totalWidth =
                    keySize +
                    spacing +
                    actionTextSize.x +
                    padding * 2.0f;

                const float totalHeight =
                    keySize +
                    padding * 2.0f;

                const ImVec2 backgroundMin(
                    centerX - totalWidth * 0.5f,

                    promptCenterY -
                    totalHeight * 0.5f
                );

                const ImVec2 backgroundMax(
                    centerX + totalWidth * 0.5f,

                    promptCenterY +
                    totalHeight * 0.5f
                );

                drawList->AddRectFilled(
                    backgroundMin,
                    backgroundMax,
                    IM_COL32(15, 17, 21, 220),
                    8.0f
                );

                drawList->AddRect(
                    backgroundMin,
                    backgroundMax,
                    IM_COL32(255, 255, 255, 45),
                    8.0f,
                    0,
                    1.0f
                );

                const ImVec2 keyMin(
                    backgroundMin.x + padding,
                    backgroundMin.y + padding
                );

                const ImVec2 keyMax(
                    keyMin.x + keySize,
                    keyMin.y + keySize
                );

                drawList->AddRectFilled(
                    keyMin,
                    keyMax,
                    IM_COL32(255, 255, 255, 235),
                    6.0f
                );

                const ImVec2 keyTextPosition(
                    keyMin.x +
                    (keySize - keyTextSize.x) *
                    0.5f,

                    keyMin.y +
                    (keySize - keyTextSize.y) *
                    0.5f
                );

                drawList->AddText(
                    keyTextPosition,
                    IM_COL32(15, 17, 21, 255),
                    keyText.c_str()
                );

                const ImVec2 actionTextPosition(
                    keyMax.x + spacing,

                    backgroundMin.y +
                    (totalHeight -
                        actionTextSize.y) *
                    0.5f
                );

                drawList->AddText(
                    actionTextPosition,
                    IM_COL32(255, 255, 255, 255),
                    actionText.c_str()
                );
            }
        }

        if (m_Runtime.IsRunning())
        {
            const ImVec2 viewportPosition =
                m_Editor.GetViewportPosition();

            const ImVec2 viewportSize =
                m_Editor.GetViewportSize();

            ImDrawList* drawList =
                ImGui::GetForegroundDrawList();

            const float centerX =
                viewportPosition.x +
                viewportSize.x * 0.5f;

            const float centerY =
                viewportPosition.y +
                viewportSize.y * 0.5f;

            const float crosshairSize =
                4.0f;

            drawList->AddCircleFilled(
                ImVec2(
                    centerX,
                    centerY
                ),
                crosshairSize,
                IM_COL32(
                    255,
                    255,
                    255,
                    220
                )
            );
        }

        UpdateLighting();

        m_Renderer.BeginFrame();

        m_Renderer.DrawGrid();

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

                m_Renderer.DrawMesh(
                    meshTransform,
                    mesh->primitive,
                    red,
                    green,
                    blue,
                    alpha,
                    texture
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
    LightComponent* sceneLight = nullptr;

    for (const Entity& entity :
        m_Scene.GetEntities())
    {
        sceneLight =
            m_Scene.GetComponent<LightComponent>(
                entity
            );

        if (sceneLight != nullptr)
        {
            break;
        }
    }

    if (sceneLight != nullptr)
    {
        m_Renderer.SetDirectionalLight(
            sceneLight->direction,
            sceneLight->color,
            sceneLight->intensity
        );
    }
    else
    {
        m_Renderer.SetDirectionalLight(
            Vec3(-0.5f, -1.0f, -0.5f),
            Vec3(1.0f, 1.0f, 1.0f),
            1.0f
        );
    }
}