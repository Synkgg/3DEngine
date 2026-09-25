#pragma once

#include "../../UI/UICanvas.h"
#include "../../UI/UILayout.h"

#include <imgui.h>
#include <string>
#include <vector>
#include <memory>

class Renderer;

class UIEditor
{
public:
    void Draw(
        UICanvas& canvas,
        Renderer& renderer
    );

    bool OpenAsset(UICanvas& canvas, const std::string& path);
    void SetVisible(bool visible);

private:
    UIWidget* m_SelectedWidget = nullptr;
    bool m_Visible = false;
    Renderer* m_Renderer = nullptr;
    std::string m_UIAssetPath = "Assets/UI/Main.ui";

    bool m_Dragging = false;
    bool m_Resizing = false;

    int m_ResizeHandle = -1;

    bool m_ShowGrid = true;
    bool m_SnapToGrid = true;
    bool m_ShowPalette = true;
    bool m_ShowHierarchy = true;
    bool m_ShowDetails = true;
    bool m_ShowWidgetBounds = true;
    char m_HierarchySearch[96]{};
    std::vector<std::string> m_UndoStack;
    std::vector<std::string> m_RedoStack;
    static constexpr std::size_t MaxHistory = 64;

    float m_GridSize = 10.0f;
    float m_Zoom = 1.0f;
    float m_DesignerScale = 1.0f;
    ImVec2 m_DesignerCanvasPosition = ImVec2(0.0f, 0.0f);

    Vec2 m_DragStartMouse;
    Vec2 m_DragStartPosition;
    Vec2 m_DragStartSize;

    void DrawHierarchy(
        UIWidget& widget,
        const std::string& search = ""
    );

    void DrawInspector(
        UIWidget& widget
    );

    void DrawToolbar(
        UICanvas& canvas
    );

    void DrawDesigner(
        UICanvas& canvas
    );

    void DrawWidget(
        UIWidget& widget,
        const UIRect& parentRect,
        const ImVec2& canvasPosition,
        float scale,
        ImDrawList* drawList
    );

    void SelectWidget(
        UIWidget* widget
    );

    void BeginDrag(
        UIWidget& widget
    );

    void UpdateDrag(
        UIWidget& widget
    );

    void BeginResize(
        UIWidget& widget,
        int handle
    );

    void UpdateResize(
        UIWidget& widget
    );

    int GetResizeHandle(
        const UIRect& rect,
        const ImVec2& mouse,
        const ImVec2& canvasPosition,
        float scale
    ) const;

    bool IsMouseInsideRect(
        const UIRect& rect,
        const ImVec2& mouse,
        const ImVec2& canvasPosition,
        float scale
    ) const;

    float SnapValue(
        float value
    ) const;

    void DeleteSelected(
        UICanvas& canvas
    );

    void DuplicateSelected(
        UICanvas& canvas
    );

    void AddWidget(
        UICanvas& canvas,
        UIWidgetType type
    );

    void RenameSelected();

    void NudgeSelected(float x, float y);
    void AlignSelected(UICanvas& canvas, int mode);
    void PushHistory(UICanvas& canvas);
    void Undo(UICanvas& canvas);
    void Redo(UICanvas& canvas);
    std::string CaptureCanvas(UICanvas& canvas) const;
    bool RestoreCanvas(UICanvas& canvas, const std::string& snapshot);
    std::unique_ptr<UIWidget> CloneWidget(const UIWidget& source) const;

    void ResetView();

    UIRect GetAbsoluteRect(
        const UIWidget& widget,
        const UIRect& canvasRect
    ) const;
};