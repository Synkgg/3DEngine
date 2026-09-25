#pragma once

#include <imgui.h>
#include <ImGuizmo.h>

#include <string>
#include <cstdint>
#include <filesystem>
#include <vector>

#include "../Scene/Entity.h"
#include "../Graphics/PrimitiveType.h"

#include "HierarchyFolder.h"

class Renderer;
class Scene;

class Editor
{
public:
    Editor();

    void Render(
        Renderer& renderer,
        Scene& scene,
        ImFont* iconFont
    );

    bool IsViewportHovered() const;

    ImVec2 GetViewportPosition() const;
    ImVec2 GetViewportSize() const;

    Entity GetSelectedEntity() const;

    bool IsPlaying() const;
    void StopPlaying();

    std::string ConsumeOpenedUIAsset();

private:
    Entity CreatePrimitiveEntity(
        Scene& scene,
        PrimitiveType primitive,
        const char* name
    );

    void ApplyEditorStyle();


    void RenderHierarchy(
        Scene& scene,
        ImFont* iconFont
    );

    void RenderViewport(
        Renderer& renderer,
        Scene& scene
    );

    void RenderInspector(
        Renderer& renderer,
        Scene& scene
    );

    void RenderContentBrowser(
        Renderer& renderer,
        Scene& scene,
        ImFont* iconFont
    );

    void RenderContentBrowserPopups(
        Scene& scene
    );

    void RenderConsole();

    Entity PickEntity(
        Renderer& renderer,
        Scene& scene,
        float ndcX,
        float ndcY
    );

    bool m_ViewportHovered;
    bool m_StyleInitialized;

    bool m_Playing;

    Entity m_SelectedEntity;

    char m_NameEditBuffer[256]{};
    std::uint32_t m_NameEditEntityID = 0;

    ImGuizmo::OPERATION m_GizmoOperation;
    ImGuizmo::MODE m_GizmoMode;

    bool m_ShowInfoLogs;
    bool m_ShowWarningLogs;
    bool m_ShowErrorLogs;
    bool m_ShowDebugLogs;
    bool m_ConsoleAutoScroll;

    std::string m_SceneFilePath;

    std::string m_ContentBrowserPath;
    std::string m_SelectedAssetPath;
    std::string m_PendingUIAssetPath;
    std::string m_MeshPreviewPath;
    bool m_ShowRenderSettings = false;

    char m_ContentBrowserSearchBuffer[256]{};

    std::vector<HierarchyFolder> m_HierarchyFolders;

    enum class AssetPopup
    {
        None,
        Rename,
        Delete,
        CreateFolder,
        CreateScene,
        CreateScript
    };

    std::string m_AssetPopupPath;
    char m_AssetPopupBuffer[256]{};

    AssetPopup m_AssetPopup =
        AssetPopup::None;

    ImVec2 m_ViewportPosition =
        ImVec2(0.0f, 0.0f);

    ImVec2 m_ViewportSize =
        ImVec2(0.0f, 0.0f);
};