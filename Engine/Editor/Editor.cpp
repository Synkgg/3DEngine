#include "Editor.h"

#include "../Graphics/Renderer.h"
#include "../Graphics/PrimitiveType.h"

#include "../Scene/Scene.h"
#include "../Scene/SceneSerializer.h"

#include "../Scene/Components/TransformComponent.h"
#include "../Scene/Components/MeshComponent.h"
#include "../Scene/Components/ColorComponent.h"
#include "../Scene/Components/NameComponent.h"
#include "../Scene/Components/PlayerComponent.h"
#include "../Scene/Components/CharacterControllerComponent.h"
#include "../Scene/Components/LightComponent.h"
#include "../Scene/Components/ColliderComponent.h"
#include "../Scene/Components/TextureComponent.h"

#include "../Graphics/Texture2D.h"

#include "../Platform/Windows/FileDialog.h"
#include "../Core/Logger.h"
#include "Fonts/IconsFontAwesome6.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

namespace
{
    constexpr float DegreesToRadians =
        0.0174532925f;

    constexpr float RadiansToDegrees =
        57.2957795f;

    std::string MakeUniqueName(
        Scene& scene,
        const std::string& baseName,
        Entity excludedEntity)
    {
        std::string name =
            baseName;

        int suffix = 2;

        while (true)
        {
            bool exists = false;

            for (const Entity& entity :
                scene.GetEntities())
            {
                if (entity.GetID() ==
                    excludedEntity.GetID())
                {
                    continue;
                }

                NameComponent* existingName =
                    scene.GetComponent<NameComponent>(
                        entity
                    );

                if (existingName != nullptr &&
                    existingName->name == name)
                {
                    exists = true;
                    break;
                }
            }

            if (!exists)
            {
                return name;
            }

            name =
                baseName +
                " " +
                std::to_string(suffix++);
        }
    }

    const char* GetLogPrefix(
        LogLevel level)
    {
        switch (level)
        {
        case LogLevel::Info:
            return "[INFO]";

        case LogLevel::Warning:
            return "[WARNING]";

        case LogLevel::Error:
            return "[ERROR]";

        case LogLevel::Debug:
            return "[DEBUG]";
        }

        return "[INFO]";
    }
}

Editor::Editor()
    : m_ViewportHovered(false),
    m_StyleInitialized(false),
    m_Playing(false),
    m_SelectedEntity(),
    m_NameEditBuffer{},
    m_NameEditEntityID(0),
    m_GizmoOperation(ImGuizmo::TRANSLATE),
    m_GizmoMode(ImGuizmo::LOCAL),
    m_ShowInfoLogs(true),
    m_ShowWarningLogs(true),
    m_ShowErrorLogs(true),
    m_ShowDebugLogs(true),
    m_ConsoleAutoScroll(true),
    m_SceneFilePath(""),
    m_ContentBrowserPath(
        (std::filesystem::current_path() / "Assets").string()
    ),
    m_SelectedAssetPath(""),
    m_ContentBrowserSearchBuffer{}
{
}

void Editor::Render(
    Renderer& renderer,
    Scene& scene,
    ImFont* iconFont)
{
    if (!m_StyleInitialized)
    {
        ApplyEditorStyle();

        m_StyleInitialized = true;
    }

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            /*
             * Save Scene
             */
            if (ImGui::MenuItem("Save Scene"))
            {
                std::string path =
                    m_SceneFilePath;

                if (path.empty())
                {
                    if (!FileDialog::SaveScene(path))
                    {
                        path.clear();
                    }
                }

                if (!path.empty())
                {
                    SceneSerializer serializer(
                        scene
                    );

                    if (serializer.Save(
                        path,
                        m_HierarchyFolders
                    ))
                    {
                        m_SceneFilePath =
                            path;

                        Logger::Info(
                            std::string(
                                "Scene saved: "
                            ) +
                            path
                        );
                    }
                    else
                    {
                        Logger::Error(
                            std::string(
                                "Failed to save scene: "
                            ) +
                            path
                        );
                    }
                }
            }

            /*
             * Save Scene As
             */
            if (ImGui::MenuItem("Save Scene As"))
            {
                std::string path;

                if (FileDialog::SaveScene(path))
                {
                    SceneSerializer serializer(
                        scene
                    );

                    if (serializer.Save(
                        path,
                        m_HierarchyFolders
                    ))
                    {
                        m_SceneFilePath =
                            path;

                        Logger::Info(
                            std::string(
                                "Scene saved as: "
                            ) +
                            path
                        );
                    }
                    else
                    {
                        Logger::Error(
                            std::string(
                                "Failed to save scene: "
                            ) +
                            path
                        );
                    }
                }
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Exit"))
            {
                Logger::Info(
                    "Exit requested."
                );
            }

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    RenderMainToolbar(renderer, scene, iconFont);

    // Reserve a real command strip above the docking workspace instead of
    // drawing controls on top of docked panels.
    ImGuiViewport* mainViewport = ImGui::GetMainViewport();
    const ImVec2 dockPos(mainViewport->WorkPos.x, mainViewport->WorkPos.y + 54.0f);
    const ImVec2 dockSize(mainViewport->WorkSize.x, std::max(1.0f, mainViewport->WorkSize.y - 54.0f));

    ImGui::SetNextWindowPos(dockPos);
    ImGui::SetNextWindowSize(dockSize);
    ImGui::SetNextWindowViewport(mainViewport->ID);

    const ImGuiWindowFlags dockHostFlags =
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("##EditorDockHost", nullptr, dockHostFlags);
    ImGui::PopStyleVar();
    ImGui::DockSpace(
        ImGui::GetID("EditorDockSpace"),
        ImVec2(0.0f, 0.0f),
        ImGuiDockNodeFlags_PassthruCentralNode
    );
    ImGui::End();

    RenderHierarchy(
        scene,
        iconFont
    );

    RenderViewport(
        renderer,
        scene
    );

    RenderInspector(
        scene
    );

    RenderConsole();

    RenderContentBrowser(
        renderer,
        scene,
        iconFont
    );
}

ImVec2 Editor::GetViewportPosition() const
{
    return m_ViewportPosition;
}

ImVec2 Editor::GetViewportSize() const
{
    return m_ViewportSize;
}

Entity Editor::GetSelectedEntity() const
{
    return m_SelectedEntity;
}

bool Editor::IsViewportHovered() const
{
    return m_ViewportHovered;
}

bool Editor::IsPlaying() const
{
    return m_Playing;
}

void Editor::StopPlaying()
{
    m_Playing = false;

    m_SelectedEntity =
        Entity();

    m_NameEditEntityID =
        0;

    m_NameEditBuffer[0] =
        '\0';

    Logger::Info(
        "Play mode stopped."
    );
}

void Editor::RenderMainToolbar(
    Renderer& renderer,
    Scene& scene,
    ImFont* iconFont)
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float menuHeight = ImGui::GetFrameHeight();

    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->Pos.y + menuHeight));
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, 54.0f));

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 8.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.025f, 0.030f, 0.038f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.10f, 0.13f, 0.16f, 1.0f));

    ImGui::Begin("##EditorCommandBar", nullptr, flags);

    auto iconButton = [&](const char* id, const char* icon, const char* tooltip, bool active = false)
    {
        if (active)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.055f, 0.30f, 0.46f, 1.0f));

        if (iconFont) ImGui::PushFont(iconFont);
        const bool pressed = ImGui::Button((std::string(icon) + "##" + id).c_str(), ImVec2(38.0f, 36.0f));
        if (iconFont) ImGui::PopFont();

        if (active)
            ImGui::PopStyleColor();

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", tooltip);

        return pressed;
    };

    if (iconButton("Save", ICON_FA_FLOPPY_DISK, "Save Scene"))
    {
        std::string path = m_SceneFilePath;
        if (path.empty())
            FileDialog::SaveScene(path);

        if (!path.empty())
        {
            SceneSerializer serializer(scene);
            if (serializer.Save(path, m_HierarchyFolders))
            {
                m_SceneFilePath = path;
                Logger::Info(std::string("Scene saved: ") + path);
            }
        }
    }

    ImGui::SameLine();
    ImGui::TextDisabled("SCENE");
    ImGui::SameLine();
    ImGui::TextUnformatted(m_SceneFilePath.empty()
        ? "Untitled"
        : std::filesystem::path(m_SceneFilePath).stem().string().c_str());

    const float transportWidth = 190.0f;
    ImGui::SameLine();
    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), (ImGui::GetWindowWidth() - transportWidth) * 0.5f));

    if (!m_Playing)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.035f, 0.34f, 0.50f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.045f, 0.46f, 0.66f, 1.0f));
        if (iconButton("Play", ICON_FA_PLAY, "Play", true))
        {
            m_Playing = true;
            m_SelectedEntity = Entity();
            m_NameEditEntityID = 0;
            m_NameEditBuffer[0] = '\0';
            Logger::Info("Play mode started.");
        }
        ImGui::PopStyleColor(2);
    }
    else
    {
        ImGui::BeginDisabled();
        iconButton("PlayDisabled", ICON_FA_PLAY, "Play");
        ImGui::EndDisabled();
    }

    ImGui::SameLine();

    ImGui::BeginDisabled(!m_Playing);
    iconButton("Pause", ICON_FA_PAUSE, "Pause is not implemented yet");
    ImGui::EndDisabled();

    ImGui::SameLine();

    if (m_Playing)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.38f, 0.10f, 0.11f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.55f, 0.13f, 0.14f, 1.0f));
        if (iconButton("Stop", ICON_FA_STOP, "Stop"))
            StopPlaying();
        ImGui::PopStyleColor(2);
    }
    else
    {
        ImGui::BeginDisabled();
        iconButton("StopDisabled", ICON_FA_STOP, "Stop");
        ImGui::EndDisabled();
    }

    const char* modeText = m_GizmoMode == ImGuizmo::LOCAL ? "Local" : "World";
    const float rightWidth = 205.0f;
    ImGui::SameLine();
    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowWidth() - rightWidth));
    ImGui::TextDisabled("TRANSFORM");
    ImGui::SameLine();
    if (ImGui::Button(modeText, ImVec2(72.0f, 36.0f)))
        m_GizmoMode = m_GizmoMode == ImGuizmo::LOCAL ? ImGuizmo::WORLD : ImGuizmo::LOCAL;

    ImGui::SameLine();
    if (iconButton("Camera", ICON_FA_CAMERA, "Reset Editor Camera"))
        renderer.ResetCamera();

    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}
