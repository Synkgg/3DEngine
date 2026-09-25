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

    // Rendering settings belong to the project, not individual scenes.
    m_ProjectSettings.EnsureLoaded();
    const RenderSettings& projectSettings = m_ProjectSettings.GetRenderSettings();
    const RenderSettings& currentSettings = renderer.GetRenderSettings();
    if (currentSettings.antiAliasing != projectSettings.antiAliasing ||
        currentSettings.antiAliasingSamples != projectSettings.antiAliasingSamples ||
        currentSettings.shadows != projectSettings.shadows || currentSettings.fog != projectSettings.fog ||
        currentSettings.bloom != projectSettings.bloom || currentSettings.viewDistance != projectSettings.viewDistance ||
        currentSettings.exposure != projectSettings.exposure || currentSettings.fogDensity != projectSettings.fogDensity ||
        currentSettings.bloomStrength != projectSettings.bloomStrength || currentSettings.shadowQuality != projectSettings.shadowQuality ||
        currentSettings.shadowDistance != projectSettings.shadowDistance)
        renderer.SetRenderSettings(projectSettings);

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

        if (ImGui::BeginMenu("Settings"))
        {
            ImGui::TextDisabled("Project Rendering");
            RenderSettings settings = m_ProjectSettings.GetRenderSettings();
            bool changed = false;
            changed |= ImGui::Checkbox("Anti-Aliasing", &settings.antiAliasing);
            if (settings.antiAliasing) { const char* names[]={"2x","4x","8x"}; int i=settings.antiAliasingSamples<=2?0:settings.antiAliasingSamples<=4?1:2; if(ImGui::Combo("MSAA",&i,names,3)){settings.antiAliasingSamples=i==0?2:i==1?4:8;changed=true;} }
            changed |= ImGui::Checkbox("Directional Shadows", &settings.shadows);
            if(settings.shadows){const char* q[]={ "Low (1024)","Medium (2048)","High (4096)" };changed|=ImGui::Combo("Shadow Quality",&settings.shadowQuality,q,3);changed|=ImGui::SliderFloat("Shadow Distance",&settings.shadowDistance,10.0f,500.0f,"%.0f");}
            changed |= ImGui::Checkbox("Bloom",&settings.bloom); if(settings.bloom) changed|=ImGui::SliderFloat("Bloom Strength",&settings.bloomStrength,0.0f,2.0f);
            changed |= ImGui::SliderFloat("Exposure",&settings.exposure,0.1f,4.0f);
            changed |= ImGui::Checkbox("Fog",&settings.fog); if(settings.fog) changed|=ImGui::SliderFloat("Fog Density",&settings.fogDensity,0.0f,0.05f,"%.4f");
            changed |= ImGui::SliderFloat("View Distance",&settings.viewDistance,25.0f,5000.0f,"%.0f");
            if(changed){ m_ProjectSettings.SetRenderSettings(settings); renderer.SetRenderSettings(settings); if(!m_ProjectSettings.Save()) Logger::Error("Failed to save project rendering settings."); }
            ImGui::EndMenu();
        }


        ImGui::EndMainMenuBar();
    }

    // Use the full main viewport for docking.
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

