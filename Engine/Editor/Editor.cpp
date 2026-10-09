#include "Editor.h"
#include <imgui_internal.h>

#include "../Graphics/Renderer.h"
#include "../Graphics/PrimitiveType.h"

#include "../Scene/Scene.h"
#include "../Scene/SceneSerializer.h"

#include "../Scene/Components/TransformComponent.h"
#include "../Scene/Components/MeshComponent.h"
#include "../Scene/Components/ColorComponent.h"
#include "../Scene/Components/NameComponent.h"
#include "../Scene/Components/PawnComponent.h"
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
    m_AssetRoot(std::filesystem::current_path() / "Assets"),
    m_SelectedAssetPath(""),
    m_ContentBrowserSearchBuffer{}
{
}

void Editor::Render(
    Renderer& renderer,
    Scene& scene,
    ImFont* iconFont,
    bool renderSceneDocument)
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

            if (ImGui::MenuItem("Export Game (Windows)...")) m_ExportRequested = true;

            if (ImGui::MenuItem("Back to Project Hub"))
            {
                m_ProjectHubRequested = true;
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

        if (ImGui::MenuItem("Settings..."))
            m_ShowRenderSettings = true;


        ImGui::EndMainMenuBar();
    }

    if (m_ShowRenderSettings)
    {
        ImGui::SetNextWindowSize(ImVec2(720.0f, 500.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
        if (ImGui::Begin("Settings", &m_ShowRenderSettings))
        {
            static int settingsPage = 0;

            ImGui::BeginChild("##SettingsCategories", ImVec2(180.0f, 0.0f), true);
            ImGui::TextDisabled("SETTINGS");
            ImGui::Spacing();
            if (ImGui::Selectable("Editor", settingsPage == 0))
                settingsPage = 0;
            if (ImGui::Selectable("Graphics", settingsPage == 1))
                settingsPage = 1;
            ImGui::EndChild();

            ImGui::SameLine();
            ImGui::BeginChild("##SettingsContent", ImVec2(0.0f, 0.0f), false);
            if (settingsPage == 0)
            {
                ImGui::TextUnformatted("Editor");
                ImGui::TextDisabled("Editor workspace and viewport preferences");
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::TextUnformatted("Viewport");
                ImGui::Checkbox("Show Grid", &m_ShowGrid);
                ImGui::SetNextItemWidth(220.0f);
                ImGui::SliderFloat("Camera Speed", &m_EditorCameraSpeed, 0.5f, 30.0f, "%.1f");
            }
            else
            {
                ImGui::TextUnformatted("Graphics");
                ImGui::TextDisabled("Project rendering settings");
                ImGui::Separator();
                ImGui::Spacing();

                RenderSettings settings = m_ProjectSettings.GetRenderSettings();
                bool changed = false;
                changed |= ImGui::Checkbox("Anti-Aliasing", &settings.antiAliasing);
                if (settings.antiAliasing)
                {
                    const char* names[] = { "2x", "4x", "8x" };
                    int sampleIndex = settings.antiAliasingSamples <= 2 ? 0 : settings.antiAliasingSamples <= 4 ? 1 : 2;
                    if (ImGui::Combo("MSAA", &sampleIndex, names, 3))
                    {
                        settings.antiAliasingSamples = sampleIndex == 0 ? 2 : sampleIndex == 1 ? 4 : 8;
                        changed = true;
                    }
                }

                changed |= ImGui::Checkbox("Directional Shadows", &settings.shadows);
                if (settings.shadows)
                {
                    const char* quality[] = { "Low (1024)", "Medium (2048)", "High (4096)" };
                    changed |= ImGui::Combo("Shadow Quality", &settings.shadowQuality, quality, 3);
                    changed |= ImGui::SliderFloat("Shadow Distance", &settings.shadowDistance, 10.0f, 500.0f, "%.0f");
                }

                changed |= ImGui::Checkbox("Bloom", &settings.bloom);
                if (settings.bloom)
                    changed |= ImGui::SliderFloat("Bloom Strength", &settings.bloomStrength, 0.0f, 2.0f);

                changed |= ImGui::SliderFloat("Exposure", &settings.exposure, 0.1f, 4.0f);
                changed |= ImGui::Checkbox("Fog", &settings.fog);
                if (settings.fog)
                    changed |= ImGui::SliderFloat("Fog Density", &settings.fogDensity, 0.0f, 0.05f, "%.4f");
                changed |= ImGui::SliderFloat("View Distance", &settings.viewDistance, 25.0f, 5000.0f, "%.0f");

                if (changed)
                {
                    m_ProjectSettings.SetRenderSettings(settings);
                    renderer.SetRenderSettings(settings);
                    if (!m_ProjectSettings.Save(m_ProjectSettingsPath.string()))
                        Logger::Error("Failed to save project rendering settings.");
                }
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }

    if (renderSceneDocument)
    {
        // Keep the scene document inside the same reserved content rectangle
        // used by asset documents. The main menu is already excluded from
        // WorkPos; reserve the document strip immediately below it.
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const float documentBarHeight = 34.0f;
        const ImVec2 sceneWorkspacePos(
            viewport->WorkPos.x,
            viewport->WorkPos.y + documentBarHeight);
        const ImVec2 sceneWorkspaceSize(
            viewport->WorkSize.x,
            std::max(1.0f, viewport->WorkSize.y - documentBarHeight));

        ImGui::SetNextWindowPos(sceneWorkspacePos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(sceneWorkspaceSize, ImGuiCond_Always);
        const ImGuiWindowFlags sceneHostFlags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoBringToFrontOnFocus;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("##SceneDocumentWorkspace", nullptr, sceneHostFlags);
        ImGui::PopStyleVar();
        const ImGuiID sceneDockspaceId = ImGui::GetID("SceneDocumentDockSpace");

        // Build the shipped layout only when no saved docking node exists.
        // Existing imgui.ini state remains authoritative for user customization.
        if (ImGui::DockBuilderGetNode(sceneDockspaceId) == nullptr)
        {
            ImGui::DockBuilderRemoveNode(sceneDockspaceId);
            ImGui::DockBuilderAddNode(sceneDockspaceId, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(sceneDockspaceId, sceneWorkspaceSize);

            ImGuiID center = sceneDockspaceId;
            ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.122f, nullptr, &center);
            ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.203f, nullptr, &center);
            ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.225f, nullptr, &center);

            ImGui::DockBuilderDockWindow("Hierarchy", left);
            ImGui::DockBuilderDockWindow("Scene", center);
            ImGui::DockBuilderDockWindow("Details", right);
            ImGui::DockBuilderDockWindow("Assets", bottom);
            ImGui::DockBuilderDockWindow("Console", bottom);
            ImGui::DockBuilderFinish(sceneDockspaceId);
        }

        ImGui::DockSpace(sceneDockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
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

bool Editor::ConsumeProjectHubRequest()
{
    const bool requested = m_ProjectHubRequested;
    m_ProjectHubRequested = false;
    return requested;
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



void Editor::ConfigureProject(const std::filesystem::path& assetRoot, const std::filesystem::path& settingsPath)
{
    m_AssetRoot = std::filesystem::absolute(assetRoot).lexically_normal();
    m_ProjectSettingsPath = std::filesystem::absolute(settingsPath).lexically_normal();
    m_ContentBrowserPath = m_AssetRoot.string();
    m_SelectedAssetPath.clear();

    if (!m_ProjectSettings.Load(settingsPath.string()))
    {
        m_ProjectSettings.Save(settingsPath.string());
    }
}

bool Editor::OpenScene(Scene& scene, const std::filesystem::path& path)
{
    std::vector<HierarchyFolder> folders;
    Scene loadedScene;
    SceneSerializer serializer(loadedScene);
    const std::string scenePath = path.lexically_normal().string();

    if (!serializer.Load(scenePath, folders))
    {
        Logger::Error("Failed to load project startup scene: " + scenePath);
        return false;
    }

    scene = std::move(loadedScene);
    m_HierarchyFolders = std::move(folders);
    m_SceneFilePath = scenePath;
    m_SelectedEntity = Entity();
    m_NameEditEntityID = 0;
    m_NameEditBuffer[0] = '\0';
    Logger::Info("Loaded scene: " + path.filename().string());
    return true;
}

bool Editor::SaveCurrentScene(Scene& scene)
{
    if (m_SceneFilePath.empty())
    {
        Logger::Error("Save the current scene before exporting it.");
        return false;
    }
    SceneSerializer serializer(scene);
    if (!serializer.Save(m_SceneFilePath, m_HierarchyFolders))
    {
        Logger::Error("Failed to save scene: " + m_SceneFilePath);
        return false;
    }
    Logger::Info("Scene saved: " + m_SceneFilePath);
    return true;
}
