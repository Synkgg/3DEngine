#include "Application.h"
#include <SDL3/SDL.h>
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
#include "../UI/UISerializer.h"
#include "../UI/UIText.h"

void Application::Run()
{
    m_Running = true;

    SDL_Event event;

    constexpr Uint64 TargetFrameTimeNS = 1000000000ull / 60ull;

    while (m_Running)
    {
        const Uint64 frameStartNS = SDL_GetTicksNS();
        const auto finishFrame = [frameStartNS, TargetFrameTimeNS]()
        {
            const Uint64 elapsedNS = SDL_GetTicksNS() - frameStartNS;
            if (elapsedNS < TargetFrameTimeNS)
                SDL_DelayPrecise(TargetFrameTimeNS - elapsedNS);
        };

        m_Time.Update();
        m_Audio.Update();

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


        // Escape must always provide an editor-level way out of Play mode.
        // A gameplay script can consume the first Escape later in this frame
        // to open its pause menu, so defer the actual stop decision until
        // after Runtime::Update. For scenes with no pause script (including
        // completely blank scenes), Escape stops Play mode immediately.
        const bool runtimeEscapePressed =
            m_Runtime.IsRunning() &&
            m_Input.IsKeyPressed(SDL_SCANCODE_ESCAPE);
         const bool stopRuntimeBeforeUpdate =
            runtimeEscapePressed &&
            m_Scene.GetEntities().empty();

        if (stopRuntimeBeforeUpdate)
        {
            m_Editor.StopPlaying();
        }

        m_ImGuiLayer.BeginFrame();

        if (m_ShowProjectHub)
        {
            ImGuiIO& hubIO = ImGui::GetIO();
            hubIO.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
            RenderProjectHub();
            m_ImGuiLayer.EndFrame();
            m_Renderer.EndFrame();
            finishFrame();
            continue;
        }

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

        // Persistent editor shell: scenes and asset editors are documents.
        // Scene rendering stays alive regardless of which document is active.
        m_Editor.Render(
            m_Renderer,
            m_Scene,
            m_ImGuiLayer.GetIconFont(),
            m_ActiveUIDocument < 0);

        if (m_Editor.ConsumeProjectHubRequest())
        {
            ReturnToProjectHub();
            m_ImGuiLayer.EndFrame();
            m_Renderer.EndFrame();
            finishFrame();
            continue;
        }

        // Consume asset-open requests before drawing the document bar so a
        // newly opened UI appears and becomes active in the same frame.
        const std::string openedUIAsset = m_Editor.ConsumeOpenedUIAsset();
        if (!openedUIAsset.empty())
        {
            const std::string normalizedPath =
                std::filesystem::absolute(std::filesystem::path(openedUIAsset))
                    .lexically_normal().generic_string();

            int existingDocument = -1;
            for (int i = 0; i < static_cast<int>(m_UIDocuments.size()); ++i)
            {
                if (m_UIDocuments[i].path == normalizedPath)
                {
                    existingDocument = i;
                    break;
                }
            }

            if (existingDocument >= 0)
            {
                m_ActiveUIDocument = existingDocument;
                m_UIDocuments[m_ActiveUIDocument].editor->Focus();
            }
            else
            {
                UIDocument document;
                document.path = normalizedPath;
                document.canvas = std::make_unique<UICanvas>();
                document.editor = std::make_unique<UIEditor>();

                if (document.editor->OpenAsset(*document.canvas, normalizedPath))
                {
                    m_UIDocuments.push_back(std::move(document));
                    m_ActiveUIDocument = static_cast<int>(m_UIDocuments.size()) - 1;
                    m_UIDocuments[m_ActiveUIDocument].editor->Focus();
                }
                else
                {
                    Logger::Error(std::string("Failed to open UI asset: ") + openedUIAsset);
                }
            }
        }

        // Reserve a second top bar below the main menu, just like ImGui's
        // main menu bar reserves space above the editor workspace.
        const float documentBarHeight = 34.0f;
        ImGuiViewport* mainViewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(mainViewport->WorkPos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(mainViewport->WorkSize.x, documentBarHeight), ImGuiCond_Always);
        ImGui::Begin("##EditorDocuments", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoBringToFrontOnFocus);

        std::string sceneLabel = m_Editor.GetSceneFilePath().empty()
            ? "Scene"
            : std::filesystem::path(m_Editor.GetSceneFilePath()).filename().string();
        if (ImGui::Selectable(sceneLabel.c_str(), m_ActiveUIDocument < 0, 0, ImVec2(140.0f, 0.0f)))
            m_ActiveUIDocument = -1;

        for (int i = 0; i < static_cast<int>(m_UIDocuments.size()); ++i)
        {
            ImGui::SameLine();
            ImGui::PushID(i);
            const std::string label = std::filesystem::path(m_UIDocuments[i].path).filename().string();
            if (ImGui::Selectable(label.c_str(), m_ActiveUIDocument == i, 0, ImVec2(160.0f, 0.0f)))
            {
                m_ActiveUIDocument = i;
                m_UIDocuments[i].editor->Focus();
            }
            ImGui::PopID();
        }
        ImGui::End();

         if (m_ActiveUIDocument >= 0 &&
            m_ActiveUIDocument < static_cast<int>(m_UIDocuments.size()))
        {
            UIDocument& document = m_UIDocuments[m_ActiveUIDocument];
            document.editor->Draw(*document.canvas, m_Renderer);
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

                m_Renderer.SetCameraMoveSpeed(m_Editor.GetEditorCameraSpeed());
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

            // Runtime canvas buttons must participate in hit testing whenever
            // the game exposes the cursor (menus, pause screens, inventory,
            // etc.). UIRenderer defaults this off so the UI editor cannot
            // accidentally consume editor clicks.
            ui.SetMouseInteractionEnabled(m_Runtime.WantsCursor());

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

            // If nobody claimed Escape by opening a pause/menu state, treat it
            // as the editor's Stop shortcut. This makes Escape reliable in
            // arbitrary scenes instead of depending on game-specific Lua.
            if (runtimeEscapePressed &&
                m_Runtime.IsRunning() &&
                !m_Runtime.IsPaused() &&
                !m_Runtime.WantsCursor())
            {
                m_Editor.StopPlaying();
            }

        }

        // Apply a Stop requested during Runtime::Update in the same frame.
        if (!m_Editor.IsPlaying() && m_Runtime.IsRunning())
        {
            StopRuntime();
        }

        UpdateLighting();

        // Directional shadow depth pass. Keep this separate from the color pass
        // so the material shader can sample a stable light-space depth map.
        for (int shadowCascade = 0; shadowCascade < Renderer::ShadowCascadeCount; ++shadowCascade)
        {
            m_Renderer.BeginShadowPass(shadowCascade);
        for (const Entity& entity : m_Scene.GetEntities())
        {
            TransformComponent* transform =
                m_Scene.GetComponent<TransformComponent>(entity);
            MeshComponent* mesh =
                m_Scene.GetComponent<MeshComponent>(entity);

            if (transform == nullptr || mesh == nullptr)
                continue;
            if (mesh->ownerNoSee && m_Runtime.IsRunning() &&
                m_Runtime.IsLocalPlayerEntityOrChild(m_Scene, entity))
                continue;

            Transform shadowTransform = m_Scene.GetWorldTransform(entity);
            shadowTransform.position.x += mesh->offset.x;
            shadowTransform.position.y += mesh->offset.y;
            shadowTransform.position.z += mesh->offset.z;
            shadowTransform.rotation.x += mesh->rotation.x;
            shadowTransform.rotation.y += mesh->rotation.y;
            shadowTransform.rotation.z += mesh->rotation.z;

            if (!mesh->modelPath.empty())
                m_Renderer.DrawShadowModel(shadowTransform, m_ProjectManager.ResolveAssetPath(mesh->modelPath));
            else
                m_Renderer.DrawShadowMesh(shadowTransform, mesh->primitive);
        }
            m_Renderer.EndShadowPass();
        }

        m_Renderer.BeginFrame();

        m_Renderer.DrawSky();

        if (!m_Runtime.IsRunning() && m_Editor.IsGridVisible())
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
                if (mesh->ownerNoSee && m_Runtime.IsRunning() &&
                    m_Runtime.IsLocalPlayerEntityOrChild(m_Scene, entity))
                    continue;

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
                            m_ProjectManager.ResolveAssetPath(textureComponent->path)
                        );
                }

                Transform meshTransform =
                    m_Scene.GetWorldTransform(entity);

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

                Texture2D* normalMap = material && !material->normalMap.empty() ? m_Renderer.LoadTexture(m_ProjectManager.ResolveAssetPath(material->normalMap)) : nullptr;
                Texture2D* metallicMap = material && !material->metallicMap.empty() ? m_Renderer.LoadTexture(m_ProjectManager.ResolveAssetPath(material->metallicMap)) : nullptr;
                Texture2D* roughnessMap = material && !material->roughnessMap.empty() ? m_Renderer.LoadTexture(m_ProjectManager.ResolveAssetPath(material->roughnessMap)) : nullptr;
                Texture2D* aoMap = material && !material->aoMap.empty() ? m_Renderer.LoadTexture(m_ProjectManager.ResolveAssetPath(material->aoMap)) : nullptr;
                Texture2D* emissiveMap = material && !material->emissiveMap.empty() ? m_Renderer.LoadTexture(m_ProjectManager.ResolveAssetPath(material->emissiveMap)) : nullptr;

                if (!mesh->modelPath.empty())
                {
                    m_Renderer.DrawModel(
                        meshTransform, m_ProjectManager.ResolveAssetPath(mesh->modelPath),
                        red, green, blue, alpha, texture,
                        material ? material->metallic : 0.0f,
                        material ? material->roughness : 0.65f,
                        material ? material->ambientOcclusion : 1.0f,
                        material ? material->emissive : 0.0f,
                        normalMap, metallicMap, roughnessMap, aoMap, emissiveMap
                    );
                }
                else
                {
                    m_Renderer.DrawMesh(
                        meshTransform, mesh->primitive,
                        red, green, blue, alpha, texture,
                        material ? material->metallic : 0.0f,
                        material ? material->roughness : 0.65f,
                        material ? material->ambientOcclusion : 1.0f,
                        material ? material->emissive : 0.0f,
                        normalMap, metallicMap, roughnessMap, aoMap, emissiveMap
                    );
                }
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

        m_Renderer.DrawDebugLines(m_Time.GetDeltaTime());

        // Finish the HDR 3D scene first. Runtime UI is intentionally
        // composited afterward so menu/text/image colors are not tone-mapped,
        // exposed, fogged, or affected by future bloom.
        m_Renderer.EndScene();

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

            m_Renderer.BeginOverlay();
            ui.Begin();

            ui.RenderCanvas(
                m_UICanvas,
                &m_Renderer
            );

            ui.End();
            m_Renderer.EndOverlay();
        }

        m_ImGuiLayer.EndFrame();

        m_Renderer.EndFrame();
        finishFrame();
    }
}
