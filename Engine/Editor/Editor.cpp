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

    // Use the full main viewport for docking. Runtime transport belongs to
    // the Scene viewport, not to a second application-wide toolbar.
    ImGui::DockSpaceOverViewport();

    RenderHierarchy(
        scene,
        iconFont
    );

    RenderViewport(
        renderer,
        scene
    );

    RenderInspector(
        renderer,
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

