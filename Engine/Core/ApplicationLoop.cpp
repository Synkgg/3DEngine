#include "Application.h"
#include "GameExporter.h"
#include "../Platform/Windows/FileDialog.h"
#include <SDL3/SDL.h>
#include <imgui.h>
#include <cstdio>
#include <cstring>
#include "../Scene/Entity.h"
#include "../Scene/Components/TransformComponent.h"
#include "../Scene/Components/MeshComponent.h"
#include "../Scene/Components/ColorComponent.h"
#include "../Scene/Components/LightComponent.h"
#include "../Scene/Components/ColliderComponent.h"
#include "../Scene/Components/TextureComponent.h"
#include "../Scene/Components/MaterialComponent.h"
#include "../Graphics/ModelAsset.h"
#include "../Graphics/PrimitiveType.h"
#include "../Core/Logger.h"
#include "../UI/UISerializer.h"
#include "../UI/UIText.h"
#include "../Editor/Fonts/IconsFontAwesome6.h"

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
            m_Input.ProcessEvent(event);
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
            m_ProjectHub.Render(
                [this](const std::string& path) { return ActivateProject(path); },
                [this](const std::string& location, const std::string& name) { return CreateProject(location, name); },
                [this]()
                {
                    m_ShowProjectHub = false;
                    SDL_SetWindowTitle(m_Window.GetNativeWindow(), "Velcryn Editor - Legacy Workspace");
                });
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

        if (!m_GameMode)
        {
        // Persistent editor shell: scenes and asset editors are documents.
        // Scene rendering stays alive regardless of which document is active.
        m_Editor.Render(
            m_Renderer,
            m_Scene,
            m_ImGuiLayer.GetIconFont(),
            m_ActiveUIDocument < 0);

        // File > Export Game: choose a destination parent, inspect the saved
        // project, and build a standalone folder without overwriting anything.
        static std::string exportStatus;
        static char exportParent[2048]{};
        static char exportFolder[256]{};
        static bool preferReleaseBuild = true;
        static bool saveSceneBeforeExport = true;
        static bool exportSucceeded = false;
        static GameExportSummary exportSummary;
        static std::string exportPreflightError;
        const auto refreshExportPreflight = [&]()
        {
            exportSummary = {};
            exportPreflightError.clear();
            if (m_ProjectManager.HasProject())
                InspectGameExport(m_ProjectManager.GetActiveProject().descriptorPath,
                                  exportSummary, exportPreflightError);
        };
        if (m_Editor.ConsumeExportRequest())
        {
            exportStatus.clear();
            exportSucceeded = false;
            if (m_ProjectManager.HasProject())
            {
                const Project& project = m_ProjectManager.GetActiveProject();
                std::snprintf(exportParent, sizeof(exportParent), "%s",
                              project.rootDirectory.parent_path().string().c_str());
                std::snprintf(exportFolder, sizeof(exportFolder), "%s-Windows",
                              project.descriptorPath.stem().string().c_str());
            }
            refreshExportPreflight();
            ImGui::OpenPopup("Export Windows game");
        }
        ImGui::SetNextWindowSize(ImVec2(650.0f, 440.0f), ImGuiCond_Appearing);
        if (ImGui::BeginPopupModal("Export Windows game", nullptr, ImGuiWindowFlags_NoResize))
        {
            ImGui::TextUnformatted("STANDALONE WINDOWS GAME");
            ImGui::TextDisabled("Vulkan / x64 - exports saved project assets and a playable executable");
            ImGui::Separator();
            if (!m_ProjectManager.HasProject())
            {
                ImGui::TextWrapped("Open a .project workspace before exporting.");
            }
            else
            {
                const Project& project = m_ProjectManager.GetActiveProject();
                ImGui::Text("Project: %s", project.name.c_str());
                ImGui::TextWrapped("Startup scene: %s", project.startupScene.generic_string().c_str());
                if (ImGui::Button("Use open scene as startup"))
                {
                    if (m_Editor.GetSceneFilePath().empty())
                        exportStatus = "Save the open scene before making it the startup scene.";
                    else if (m_Editor.IsPlaying())
                        exportStatus = "Stop Play mode before saving or changing the startup scene.";
                    else if (!m_Editor.SaveCurrentScene(m_Scene))
                        exportStatus = "Could not save the current scene.";
                    else if (!m_ProjectManager.SetStartupScene(m_Editor.GetSceneFilePath()))
                        exportStatus = "The startup scene must be a saved .scene inside this project.";
                    else
                    {
                        exportStatus = "Startup scene updated and saved.";
                        refreshExportPreflight();
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Recheck project"))
                    refreshExportPreflight();
                if (!exportPreflightError.empty())
                    ImGui::TextWrapped("Preflight: %s", exportPreflightError.c_str());
                else
                    ImGui::Text("Ready: %llu asset files (%.1f MiB)",
                                static_cast<unsigned long long>(exportSummary.assetFileCount),
                                static_cast<double>(exportSummary.assetBytes) / (1024.0 * 1024.0));

                ImGui::Separator();
                ImGui::TextUnformatted("Destination parent folder");
                ImGui::SetNextItemWidth(510.0f);
                ImGui::InputText("##ExportParent", exportParent, sizeof(exportParent));
                ImGui::SameLine();
                if (ImGui::Button("Browse..."))
                {
                    std::string folder;
                    if (FileDialog::SelectFolder(folder))
                        std::snprintf(exportParent, sizeof(exportParent), "%s", folder.c_str());
                }
                ImGui::TextUnformatted("New package folder name");
                ImGui::SetNextItemWidth(350.0f);
                ImGui::InputText("##ExportFolder", exportFolder, sizeof(exportFolder));

                ImGui::Checkbox("Save the open scene before export", &saveSceneBeforeExport);
                ImGui::Checkbox("Prefer a Release executable when available", &preferReleaseBuild);

                const std::filesystem::path currentExe =
                    std::filesystem::path(SDL_GetBasePath()) / "VelcrynEditor.exe";
                const std::filesystem::path releaseExe =
                    currentExe.parent_path().parent_path() / "x64-Release" / "VelcrynEditor.exe";
                const bool releaseAvailable = std::filesystem::is_regular_file(releaseExe);
                const bool runningRelease = currentExe.parent_path().filename() == "x64-Release";
                const std::filesystem::path selectedExe =
                    preferReleaseBuild && releaseAvailable ? releaseExe : currentExe;
                ImGui::TextWrapped("Executable: %s", selectedExe.string().c_str());
                if (!runningRelease && (!preferReleaseBuild || !releaseAvailable))
                    ImGui::TextWrapped("Note: exporting the running build. Build x64-Release for a smaller, optimized distribution.");
                ImGui::TextDisabled("Saved UI documents and other assets must be saved separately.");

                const std::string folderName(exportFolder);
                const bool validName = !folderName.empty() && folderName != "." && folderName != ".." &&
                    folderName.find_first_of("<>:\"/\\|?*") == std::string::npos &&
                    folderName.back() != '.' && folderName.back() != ' ';
                const bool canExport = validName && exportParent[0] != '\0' &&
                    !m_Editor.IsPlaying() && exportPreflightError.empty();
                if (!validName)
                    ImGui::TextUnformatted("Choose a valid Windows folder name.");
                if (m_Editor.IsPlaying())
                    ImGui::TextUnformatted("Stop Play mode before exporting.");

                ImGui::BeginDisabled(!canExport);
                if (ImGui::Button("Export Game", ImVec2(180.0f, 36.0f)))
                {
                    exportSucceeded = false;
                    exportStatus.clear();
                    if (saveSceneBeforeExport && !m_Editor.GetSceneFilePath().empty() &&
                        !m_Editor.SaveCurrentScene(m_Scene))
                        exportStatus = "Export canceled: the open scene could not be saved.";
                    else if (!m_Editor.GetProjectSettings().Save(project.GetSettingsPath().string()))
                        exportStatus = "Export canceled: project settings could not be saved.";
                    else
                    {
                        const std::filesystem::path output =
                            std::filesystem::path(exportParent) / folderName;
                        std::string error;
                        exportSucceeded = ExportGame(project.descriptorPath, output, selectedExe, error);
                        exportStatus = exportSucceeded ? "Export complete: " + output.string()
                                                       : "Export failed: " + error;
                        refreshExportPreflight();
                    }
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                if (ImGui::Button("Close", ImVec2(100.0f, 36.0f)))
                    ImGui::CloseCurrentPopup();
                if (!exportStatus.empty())
                {
                    ImGui::Separator();
                    ImGui::TextWrapped("%s", exportStatus.c_str());
                }
                if (exportSucceeded)
                    ImGui::TextDisabled("Run Game.exe from the exported folder. Share the entire folder.");
            }
            if (!m_ProjectManager.HasProject() && ImGui::Button("Close"))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

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

        // Document strip: compact editor chrome shared by scene and asset pages.
        const float documentBarHeight = 34.0f;
        ImGuiViewport* mainViewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(mainViewport->WorkPos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(mainViewport->WorkSize.x, documentBarHeight), ImGuiCond_Always);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(7.0f, 4.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(3.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(27, 29, 32, 255));
        ImGui::Begin("##EditorDocuments", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoBringToFrontOnFocus);

        auto drawDocumentTab = [&](const char* icon, const std::string& label, bool selected, float width, bool closable)
        {
            ImGui::PushID(label.c_str());
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            const ImVec2 size(width, 26.0f);
            const bool hovered = ImGui::IsMouseHoveringRect(pos, ImVec2(pos.x + size.x, pos.y + size.y));

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            if (selected)
                drawList->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                    IM_COL32(46, 49, 55, 255), 4.0f);
            else if (hovered)
                drawList->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                    IM_COL32(37, 40, 45, 255), 4.0f);

            if (selected)
                drawList->AddRectFilled(
                    ImVec2(pos.x + 7.0f, pos.y + size.y - 2.0f),
                    ImVec2(pos.x + size.x - 7.0f, pos.y + size.y),
                    IM_COL32(92, 153, 230, 255), 1.0f);

            ImGui::InvisibleButton("##DocumentTab", size);
            const bool clicked = ImGui::IsItemClicked();

            bool closeClicked = false;
            if (closable)
            {
                const float closeSize = 16.0f;
                const ImVec2 closePos(pos.x + size.x - closeSize - 6.0f, pos.y + 5.0f);
                const bool closeHovered = ImGui::IsMouseHoveringRect(
                    closePos, ImVec2(closePos.x + closeSize, closePos.y + closeSize));
                if (closeHovered)
                {
                    drawList->AddRectFilled(closePos,
                        ImVec2(closePos.x + closeSize, closePos.y + closeSize),
                        IM_COL32(58, 62, 69, 255), 3.0f);
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                        closeClicked = true;
                }

                const ImU32 closeColor = closeHovered
                    ? IM_COL32(235, 237, 240, 255)
                    : IM_COL32(130, 135, 143, 255);
                const ImVec2 c(closePos.x + closeSize * 0.5f, closePos.y + closeSize * 0.5f);
                drawList->AddLine(ImVec2(c.x - 3.0f, c.y - 3.0f), ImVec2(c.x + 3.0f, c.y + 3.0f), closeColor, 1.25f);
                drawList->AddLine(ImVec2(c.x + 3.0f, c.y - 3.0f), ImVec2(c.x - 3.0f, c.y + 3.0f), closeColor, 1.25f);
            }

            float x = pos.x + 9.0f;
            if (m_ImGuiLayer.GetIconFont())
            {
                ImGui::PushFont(m_ImGuiLayer.GetIconFont());
                const float iconSize = ImGui::GetFontSize() * 0.66f;
                drawList->AddText(ImGui::GetFont(), iconSize,
                    ImVec2(x, pos.y + (size.y - iconSize) * 0.5f),
                    selected ? IM_COL32(205, 221, 242, 255) : IM_COL32(145, 151, 160, 255),
                    icon);
                const float iconWidth =
                    ImGui::GetFont()->CalcTextSizeA(iconSize, FLT_MAX, 0.0f, icon).x;
                ImGui::PopFont();
                x += iconWidth + 7.0f;
            }

            const ImU32 textColor = selected
                ? IM_COL32(235, 237, 240, 255)
                : IM_COL32(178, 182, 188, 255);
            const ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
            const float available = std::max(0.0f, pos.x + size.x - x - (closable ? 30.0f : 8.0f));
            drawList->PushClipRect(ImVec2(x, pos.y), ImVec2(x + available, pos.y + size.y), true);
            drawList->AddText(ImVec2(x, pos.y + (size.y - textSize.y) * 0.5f), textColor, label.c_str());
            drawList->PopClipRect();

            ImGui::PopID();
            return closeClicked ? 2 : clicked ? 1 : 0;
        };

        std::string sceneLabel = m_Editor.GetSceneFilePath().empty()
            ? "Scene"
            : std::filesystem::path(m_Editor.GetSceneFilePath()).filename().string();
        if (drawDocumentTab(ICON_FA_CUBES, sceneLabel, m_ActiveUIDocument < 0, 150.0f, false) == 1)
            m_ActiveUIDocument = -1;

        for (int i = 0; i < static_cast<int>(m_UIDocuments.size()); ++i)
        {
            ImGui::SameLine();
            ImGui::PushID(i);
            const std::string label = std::filesystem::path(m_UIDocuments[i].path).filename().string();
            const int tabAction = drawDocumentTab(
                ICON_FA_OBJECT_GROUP, label, m_ActiveUIDocument == i, 165.0f, true);
            ImGui::PopID();

            if (tabAction == 2)
            {
                const bool wasActive = m_ActiveUIDocument == i;
                m_UIDocuments.erase(m_UIDocuments.begin() + i);

                if (m_UIDocuments.empty())
                    m_ActiveUIDocument = -1;
                else if (wasActive)
                    m_ActiveUIDocument = std::min(i, static_cast<int>(m_UIDocuments.size()) - 1);
                else if (m_ActiveUIDocument > i)
                    --m_ActiveUIDocument;

                --i;
                continue;
            }

            if (tabAction == 1)
            {
                m_ActiveUIDocument = i;
                m_UIDocuments[i].editor->Focus();
            }
        }

        const ImVec2 windowPos = ImGui::GetWindowPos();
        const ImVec2 windowSize = ImGui::GetWindowSize();
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(windowPos.x, windowPos.y + windowSize.y - 1.0f),
            ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y - 1.0f),
            IM_COL32(48, 51, 57, 255));

        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);

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

        }
        else
        {
            int width = 0, height = 0;
            SDL_GetWindowSizeInPixels(m_Window.GetNativeWindow(), &width, &height);
            if (width > 0 && height > 0) m_Renderer.ResizeViewport(width, height);
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
            int logicalWidth = 0, logicalHeight = 0;
            SDL_GetWindowSize(m_Window.GetNativeWindow(), &logicalWidth, &logicalHeight);
            const ImVec2 gameViewportPosition = m_GameMode ? ImVec2(0, 0) : m_Editor.GetViewportPosition();
            const ImVec2 gameViewportSize = m_GameMode ? ImVec2(float(logicalWidth), float(logicalHeight)) : m_Editor.GetViewportSize();

            // Viewport position/size and Input mouse coordinates are both in
            // SDL/ImGui logical window coordinates here. UIRenderer performs
            // the logical-canvas mapping itself, so do not scale this rectangle
            // to the offscreen Vulkan render-target dimensions a second time.
            ui.SetLogicalSize(canvasSize.x, canvasSize.y);
            ui.UpdateInput(
                m_UICanvas,
                m_Input,
                gameViewportPosition.x,
                gameViewportPosition.y,
                gameViewportSize.x,
                gameViewportSize.y,
                m_Time.GetDeltaTime()
            );
            if (ui.HasTextInputFocus()) {
                if (!SDL_TextInputActive(m_Window.GetNativeWindow())) SDL_StartTextInput(m_Window.GetNativeWindow());
            } else if (SDL_TextInputActive(m_Window.GetNativeWindow())) SDL_StopTextInput(m_Window.GetNativeWindow());

            m_Runtime.Update(
                m_Scene,
                m_Renderer,
                m_Input,
                m_Time.GetDeltaTime()
            );

            // If nobody claimed Escape by opening a pause/menu state, treat it
            // as the editor's Stop shortcut. This makes Escape reliable in
            // arbitrary scenes instead of depending on game-specific Lua.
            if (!m_GameMode && runtimeEscapePressed &&
                m_Runtime.IsRunning() &&
                !m_Runtime.IsPaused() &&
                !m_Runtime.WantsCursor())
            {
                m_Editor.StopPlaying();
            }

        }

        // Apply a Stop requested during Runtime::Update in the same frame.
        if (!m_GameMode && !m_Editor.IsPlaying() && m_Runtime.IsRunning())
        {
            StopRuntime();
        }

        UpdateLighting();

        // Acquire/reset the Vulkan frame before any scene, shadow, overlay, or
        // ImGui GPU work is recorded for this frame.
        m_Renderer.BeginFrame();

        // Directional shadow depth pass. Keep this separate from the color pass
        // so the material shader can sample a stable light-space depth map.
        for (int shadowCascade = 0; shadowCascade < Renderer::ShadowCascadeCount; ++shadowCascade)
        {
            if (!m_Renderer.BeginShadowPass(shadowCascade)) continue;
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
            {
                const std::string resolved=m_ProjectManager.ResolveAssetPath(mesh->modelPath);
                ModelAsset* asset=m_Renderer.GetModelAsset(resolved);
                if(asset&&asset->IsSkeletal()&&!asset->animations.empty())m_Renderer.DrawAnimatedShadowModel(shadowTransform,resolved,(std::size_t)std::max(mesh->animationClip,0),mesh->animationTime,mesh->animationLoop);
                else m_Renderer.DrawShadowModel(shadowTransform,resolved);
            }
            else
                m_Renderer.DrawShadowMesh(shadowTransform, mesh->primitive);
        }
            m_Renderer.EndShadowPass();
        }

        m_Renderer.DrawSky();

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
                    const std::string resolvedModel=m_ProjectManager.ResolveAssetPath(mesh->modelPath);
                    ModelAsset* modelAsset=m_Renderer.GetModelAsset(resolvedModel);
                    if(modelAsset&&modelAsset->IsSkeletal()&&!modelAsset->animations.empty())
                    {
                        if(mesh->animationPlaying)mesh->animationTime+=m_Time.GetDeltaTime()*mesh->animationSpeed;
                        m_Renderer.DrawAnimatedModel(meshTransform, resolvedModel, (std::size_t)std::max(mesh->animationClip,0),
                            mesh->animationTime, mesh->animationLoop, red, green, blue, alpha, texture,
                            material ? material->metallic : 0.0f, material ? material->roughness : 0.65f,
                            material ? material->ambientOcclusion : 1.0f, material ? material->emissive : 0.0f,
                            normalMap, metallicMap, roughnessMap, aoMap, emissiveMap, material != nullptr);
                    }
                    else m_Renderer.DrawModel(
                        meshTransform, resolvedModel,
                        red, green, blue, alpha, texture,
                        material ? material->metallic : 0.0f,
                        material ? material->roughness : 0.65f,
                        material ? material->ambientOcclusion : 1.0f,
                        material ? material->emissive : 0.0f,
                        normalMap, metallicMap, roughnessMap, aoMap, emissiveMap, material != nullptr
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

        if (!m_Runtime.IsRunning() && m_Editor.IsGridVisible())
            m_Renderer.DrawGrid();

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

        if (m_GameMode)
        {
            const auto* viewport = ImGui::GetMainViewport();
            ImGui::GetBackgroundDrawList()->AddImage(
                (ImTextureID)m_Renderer.GetViewportTexture(), viewport->Pos,
                ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y));
        }
        m_ImGuiLayer.EndFrame();

        m_Renderer.EndFrame();
        if (m_FrameLimit > 0 && --m_FrameLimit == 0) m_Running = false;
        finishFrame();
    }
}
