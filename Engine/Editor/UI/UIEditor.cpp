#include "UIEditor.h"

#include "../../UI/UIWidget.h"
#include "../../UI/UIWidgetFactory.h"
#include "../../UI/UIPanel.h"
#include "../../UI/UIText.h"
#include "../../UI/UIImage.h"
#include "../../UI/UIButton.h"
#include "../../UI/UISerializer.h"
#include "../../Graphics/Renderer.h"
#include "../../Graphics/Texture2D.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <filesystem>
#include <vector>
#include <functional>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <chrono>

bool UIEditor::OpenAsset(UICanvas& canvas, const std::string& path)
{
    m_SelectedWidget = nullptr;
    m_Visible = true;

    if (!UISerializer::Load(canvas, path))
        return false;

    m_UIAssetPath = std::filesystem::path(path).generic_string();
    ResetView();
    return true;
}

void UIEditor::SetVisible(bool visible)
{
    m_Visible = visible;
    if (!visible)
    {
        m_SelectedWidget = nullptr;
        m_Dragging = false;
        m_Resizing = false;
        m_ResizeHandle = -1;
    }
}

void UIEditor::Draw(
    UICanvas& canvas,
    Renderer& renderer)
{
    if (!m_Visible)
        return;

    m_Renderer = &renderer;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("Widget Blueprint");
    ImGui::PopStyleVar();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(24, 26, 29, 255));
    ImGui::BeginChild("WidgetToolbar", ImVec2(0.0f, 42.0f), false);
    ImGui::SetCursorPos(ImVec2(8.0f, 7.0f));
    DrawToolbar(canvas);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    const ImVec2 available = ImGui::GetContentRegionAvail();
    const bool compact = available.x < 1050.0f;
    const bool veryCompact = available.x < 760.0f;

    // On narrow windows the designer gets priority. Side panels become
    // optional instead of squeezing a 16:9 canvas into an unusable strip.
    bool showPalette = m_ShowPalette && !veryCompact;
    bool showHierarchy = m_ShowHierarchy && !veryCompact;
    bool showDetails = m_ShowDetails && !compact;

    float paletteWidth = showPalette ? std::clamp(available.x * 0.14f, 135.0f, 175.0f) : 0.0f;
    float hierarchyWidth = showHierarchy ? std::clamp(available.x * 0.18f, 165.0f, 225.0f) : 0.0f;
    float inspectorWidth = showDetails ? std::clamp(available.x * 0.24f, 245.0f, 310.0f) : 0.0f;

    float used = paletteWidth + hierarchyWidth + inspectorWidth;
    float designerWidth = std::max(280.0f, available.x - used);

    auto sameLine = []() { ImGui::SameLine(0.0f, 0.0f); };

    if (showPalette)
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(29, 31, 35, 255));
        ImGui::BeginChild("Palette", ImVec2(paletteWidth, 0.0f), true);
        ImGui::TextDisabled("PALETTE");
        ImGui::Separator();
        static char paletteSearch[64] = {};
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##PaletteSearch", "Search widgets...", paletteSearch, sizeof(paletteSearch));
        std::string paletteQuery = paletteSearch;
        std::transform(paletteQuery.begin(), paletteQuery.end(), paletteQuery.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        auto paletteItem = [&](const char* label, UIWidgetType type)
        {
            std::string lower = label;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (!paletteQuery.empty() && lower.find(paletteQuery) == std::string::npos)
                return;
            if (ImGui::Selectable(label))
                AddWidget(canvas, type);
            if (ImGui::BeginDragDropSource())
            {
                const int payloadType = static_cast<int>(type);
                ImGui::SetDragDropPayload("UI_PALETTE_WIDGET", &payloadType, sizeof(payloadType));
                ImGui::Text("Add %s", label);
                ImGui::EndDragDropSource();
            }
        };
        paletteItem("Panel", UIWidgetType::Panel);
        paletteItem("Text", UIWidgetType::Text);
        paletteItem("Image", UIWidgetType::Image);
        paletteItem("Button", UIWidgetType::Button);
        ImGui::Spacing();
        ImGui::TextDisabled("Click to add, or drag into the Designer");
        ImGui::EndChild();
        ImGui::PopStyleColor();
        sameLine();
    }

    if (showHierarchy)
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(24, 26, 30, 255));
        ImGui::BeginChild("HierarchyPanel", ImVec2(hierarchyWidth, 0.0f), true);
        ImGui::TextDisabled("HIERARCHY");
        ImGui::Separator();
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##UIHierarchySearch", "Search hierarchy...", m_HierarchySearch, sizeof(m_HierarchySearch));
        std::string hierarchyQuery=m_HierarchySearch;
        std::transform(hierarchyQuery.begin(),hierarchyQuery.end(),hierarchyQuery.begin(),
            [](unsigned char c){return static_cast<char>(std::tolower(c));});
        if (canvas.GetRoot()) DrawHierarchy(*canvas.GetRoot(), hierarchyQuery);
        ImGui::EndChild();
        ImGui::PopStyleColor();
        sameLine();
    }

    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(17, 18, 21, 255));
    ImGui::BeginChild("DesignerPanel", ImVec2(designerWidth, 0.0f), true);
    DrawDesigner(canvas);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    if (showDetails)
    {
        sameLine();
        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(29, 31, 35, 255));
        ImGui::BeginChild("DetailsPanel", ImVec2(inspectorWidth, 0.0f), true);
        ImGui::TextDisabled("DETAILS");
        ImGui::Separator();
        if (m_SelectedWidget)
            DrawInspector(*m_SelectedWidget);
        else
            ImGui::TextDisabled("Select a widget to edit its properties.");
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
    {
        const bool editingText = ImGui::GetIO().WantTextInput;
        const bool ctrl = ImGui::GetIO().KeyCtrl;
        if (!editingText && ImGui::IsKeyPressed(ImGuiKey_Delete)) { PushHistory(canvas); DeleteSelected(canvas); }
        if (!editingText && ctrl && ImGui::IsKeyPressed(ImGuiKey_D)) { PushHistory(canvas); DuplicateSelected(canvas); }
        if (!editingText && ctrl && ImGui::IsKeyPressed(ImGuiKey_Z)) Undo(canvas);
        if (!editingText && ctrl && ImGui::IsKeyPressed(ImGuiKey_Y)) Redo(canvas);
        if (!editingText && ImGui::IsKeyPressed(ImGuiKey_F2)) RenameSelected();
        if (!editingText && ImGui::IsKeyPressed(ImGuiKey_F)) ResetView();
        const float nudge=ImGui::GetIO().KeyShift ? std::max(1.0f,m_GridSize) : 1.0f;
        if(!editingText && ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) NudgeSelected(-nudge,0);
        if(!editingText && ImGui::IsKeyPressed(ImGuiKey_RightArrow)) NudgeSelected(nudge,0);
        if(!editingText && ImGui::IsKeyPressed(ImGuiKey_UpArrow)) NudgeSelected(0,-nudge);
        if(!editingText && ImGui::IsKeyPressed(ImGuiKey_DownArrow)) NudgeSelected(0,nudge);
    }

    ImGui::End();
}

void UIEditor::DrawHierarchy(
    UIWidget& widget,
    const std::string& search)
{
    ImGuiTreeNodeFlags flags =
        ImGuiTreeNodeFlags_OpenOnArrow |
        ImGuiTreeNodeFlags_OpenOnDoubleClick;

    if (widget.GetChildren().empty())
    {
        flags |=
            ImGuiTreeNodeFlags_Leaf;
    }

    if (m_SelectedWidget ==
        &widget)
    {
        flags |=
            ImGuiTreeNodeFlags_Selected;
    }

    std::string label =
        widget.GetName();

    if (label.empty())
    {
        label = "Widget";
    }

    std::string lowerLabel=label;
    std::transform(lowerLabel.begin(),lowerLabel.end(),lowerLabel.begin(),
        [](unsigned char c){return static_cast<char>(std::tolower(c));});
    std::function<bool(const UIWidget&)> matches=[&](const UIWidget& w)
    {
        std::string n=w.GetName();
        std::transform(n.begin(),n.end(),n.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
        if(search.empty() || n.find(search)!=std::string::npos) return true;
        for(const auto& child:w.GetChildren()) if(child && matches(*child)) return true;
        return false;
    };
    if(!matches(widget)) return;
    if(!search.empty()) flags |= ImGuiTreeNodeFlags_DefaultOpen;

    const char* typeName = widget.GetType()==UIWidgetType::Panel ? "[Panel]" :
        widget.GetType()==UIWidgetType::Text ? "[Text]" :
        widget.GetType()==UIWidgetType::Image ? "[Image]" : "[Button]";
    const bool open =
        ImGui::TreeNodeEx(
            &widget,
            flags,
            "%s  %s%s",
            typeName,
            widget.IsVisible() ? "" : "(hidden) ",
            label.c_str()
        );

    if (ImGui::IsItemClicked(
        ImGuiMouseButton_Left))
    {
        SelectWidget(
            &widget
        );
    }

    if (ImGui::BeginDragDropSource())
    {
        UIWidget* draggedWidget = &widget;
        ImGui::SetDragDropPayload("UI_WIDGET_REPARENT", &draggedWidget, sizeof(draggedWidget));
        ImGui::Text("Move %s", label.c_str());
        ImGui::EndDragDropSource();
    }

    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("UI_WIDGET_REPARENT"))
        {
            if (payload->DataSize == sizeof(UIWidget*))
            {
                UIWidget* draggedWidget = *static_cast<UIWidget* const*>(payload->Data);
                const bool targetCanContainChildren =
                    widget.GetType() == UIWidgetType::Panel;
                if (draggedWidget && draggedWidget != &widget &&
                    draggedWidget->GetParent() != &widget &&
                    targetCanContainChildren &&
                    !widget.IsDescendantOf(draggedWidget))
                {
                    UIWidget* oldParent = draggedWidget->GetParent();
                    if (oldParent)
                    {
                        std::unique_ptr<UIWidget> moved = oldParent->DetachChild(draggedWidget);
                        if (moved)
                        {
                            widget.AddChild(std::move(moved));
                            SelectWidget(draggedWidget);
                        }
                    }
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    if (open)
    {
        for (const auto& child :
            widget.GetChildren())
        {
            if (child)
            {
                DrawHierarchy(
                    *child,
                    search
                );
            }
        }

        ImGui::TreePop();
    }
}

void UIEditor::DrawInspector(
    UIWidget& widget)
{
    char nameBuffer[256];

    std::snprintf(
        nameBuffer,
        sizeof(nameBuffer),
        "%s",
        widget.GetName().c_str()
    );

    if (ImGui::InputText(
        "Name",
        nameBuffer,
        sizeof(nameBuffer)))
    {
        widget.SetName(
            nameBuffer
        );
    }

    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Layout", ImGuiTreeNodeFlags_DefaultOpen))
    {
    Vec2 position =
        widget.GetPosition();

    if (ImGui::DragFloat2(
        "Position",
        &position.x,
        1.0f))
    {
        widget.SetPosition(
            position
        );
    }

    Vec2 size =
        widget.GetSize();

    if (ImGui::DragFloat2(
        "Size",
        &size.x,
        1.0f,
        1.0f,
        10000.0f))
    {
        size.x =
            std::max(
                1.0f,
                size.x
            );

        size.y =
            std::max(
                1.0f,
                size.y
            );

        widget.SetSize(
            size
        );
    }

    Vec2 anchorMin = widget.GetAnchorMinimum();
    Vec2 anchorMax = widget.GetAnchorMaximum();

    if (ImGui::DragFloat2("Anchor Min", &anchorMin.x, 0.01f, 0.0f, 1.0f))
    {
        anchorMin.x = std::clamp(anchorMin.x, 0.0f, 1.0f);
        anchorMin.y = std::clamp(anchorMin.y, 0.0f, 1.0f);
        anchorMax.x = std::max(anchorMax.x, anchorMin.x);
        anchorMax.y = std::max(anchorMax.y, anchorMin.y);
        widget.SetAnchors(anchorMin, anchorMax);
    }

    if (ImGui::DragFloat2("Anchor Max", &anchorMax.x, 0.01f, 0.0f, 1.0f))
    {
        anchorMax.x = std::clamp(anchorMax.x, anchorMin.x, 1.0f);
        anchorMax.y = std::clamp(anchorMax.y, anchorMin.y, 1.0f);
        widget.SetAnchors(anchorMin, anchorMax);
    }

    ImGui::TextDisabled("Anchor Presets");
    if (ImGui::Button("Top Left")) { widget.SetAnchor(Vec2(0,0)); widget.SetPivot(Vec2(0,0)); }
    ImGui::SameLine();
    if (ImGui::Button("Top")) { widget.SetAnchor(Vec2(.5f,0)); widget.SetPivot(Vec2(.5f,0)); }
    ImGui::SameLine();
    if (ImGui::Button("Top Right")) { widget.SetAnchor(Vec2(1,0)); widget.SetPivot(Vec2(1,0)); }
    if (ImGui::Button("Left")) { widget.SetAnchor(Vec2(0,.5f)); widget.SetPivot(Vec2(0,.5f)); }
    ImGui::SameLine();
    if (ImGui::Button("Center")) { widget.SetAnchor(Vec2(.5f,.5f)); widget.SetPivot(Vec2(.5f,.5f)); }
    ImGui::SameLine();
    if (ImGui::Button("Right")) { widget.SetAnchor(Vec2(1,.5f)); widget.SetPivot(Vec2(1,.5f)); }
    if (ImGui::Button("Bottom Left")) { widget.SetAnchor(Vec2(0,1)); widget.SetPivot(Vec2(0,1)); }
    ImGui::SameLine();
    if (ImGui::Button("Bottom")) { widget.SetAnchor(Vec2(.5f,1)); widget.SetPivot(Vec2(.5f,1)); }
    ImGui::SameLine();
    if (ImGui::Button("Bottom Right")) { widget.SetAnchor(Vec2(1,1)); widget.SetPivot(Vec2(1,1)); }
    if (ImGui::Button("Fill Width")) { widget.SetAnchors(Vec2(0,anchorMin.y),Vec2(1,anchorMax.y)); }
    ImGui::SameLine();
    if (ImGui::Button("Fill Height")) { widget.SetAnchors(Vec2(anchorMin.x,0),Vec2(anchorMax.x,1)); }
    ImGui::SameLine();
    if (ImGui::Button("Fill")) { widget.SetAnchors(Vec2(0,0),Vec2(1,1)); widget.SetPivot(Vec2(0,0)); }

    Vec2 pivot =
        widget.GetPivot();

    if (ImGui::DragFloat2(
        "Pivot",
        &pivot.x,
        0.01f,
        0.0f,
        1.0f))
    {
        pivot.x =
            std::clamp(
                pivot.x,
                0.0f,
                1.0f
            );

        pivot.y =
            std::clamp(
                pivot.y,
                0.0f,
                1.0f
            );

        widget.SetPivot(
            pivot
        );
    }

    }

    ImGui::Spacing();
    ImGui::SeparatorText("Appearance");

    Vec4 color =
        widget.GetColor();

    if (ImGui::ColorEdit4(
        "Color",
        &color.x))
    {
        widget.SetColor(
            color
        );
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Behavior");

    bool visible =
        widget.IsVisible();

    if (ImGui::Checkbox(
        "Visible",
        &visible))
    {
        widget.SetVisible(
            visible
        );
    }

    bool enabled = widget.IsEnabled();
    if (ImGui::Checkbox("Enabled", &enabled))
        widget.SetEnabled(enabled);

    bool hitTest = widget.IsHitTestVisible();
    if (ImGui::Checkbox("Hit Test Visible", &hitTest))
        widget.SetHitTestVisible(hitTest);

    int zOrder = widget.GetZOrder();
    if (ImGui::DragInt("Z Order", &zOrder, 1.0f, -1000, 1000))
        widget.SetZOrder(zOrder);

    if (UIText* text =
        dynamic_cast<UIText*>(
            &widget))
    {
        ImGui::TextUnformatted(
            "Text"
        );

        char textBuffer[1024];

        std::snprintf(
            textBuffer,
            sizeof(textBuffer),
            "%s",
            text->GetText().c_str()
        );

        if (ImGui::InputText(
            "Content",
            textBuffer,
            sizeof(textBuffer)))
        {
            text->SetText(
                textBuffer
            );
        }

        float fontSize =
            text->GetFontSize();

        if (ImGui::DragFloat(
            "Font Size",
            &fontSize,
            0.5f,
            1.0f,
            512.0f))
        {
            text->SetFontSize(
                fontSize
            );
        }
    }

    if (UIImage* image =
        dynamic_cast<UIImage*>(&widget))
    {
        ImGui::Spacing();
        ImGui::SeparatorText("Appearance");
        ImGui::TextDisabled("Brush / Image");

        const std::string currentPath = image->GetTexturePath();
        const std::string preview = currentPath.empty()
            ? "None"
            : std::filesystem::path(currentPath).filename().string();

        if (ImGui::BeginCombo("Image Asset", preview.c_str()))
        {
            static char imageSearch[128] = {};
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint("##UIImageSearch", "Search textures...", imageSearch, sizeof(imageSearch));
            ImGui::Separator();

            if (ImGui::Selectable("None", currentPath.empty()))
                image->SetTexturePath("");

            std::string query = imageSearch;
            std::transform(query.begin(), query.end(), query.begin(),
                [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

            std::error_code ec;
            const std::filesystem::path root("Assets");
            if (std::filesystem::exists(root, ec))
            {
                for (const auto& entry : std::filesystem::recursive_directory_iterator(root, ec))
                {
                    if (ec) break;
                    if (!entry.is_regular_file()) continue;

                    std::string ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(),
                        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
                    if (ext != ".png" && ext != ".jpg" && ext != ".jpeg" &&
                        ext != ".bmp" && ext != ".tga")
                        continue;

                    const std::string assetPath = entry.path().generic_string();
                    std::string searchable = assetPath;
                    std::transform(searchable.begin(), searchable.end(), searchable.begin(),
                        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
                    if (!query.empty() && searchable.find(query) == std::string::npos)
                        continue;

                    ImGui::PushID(assetPath.c_str());
                    Texture2D* texture = m_Renderer ? m_Renderer->LoadTexture(assetPath) : nullptr;
                    if (texture && texture->IsLoaded())
                    {
                        ImGui::Image((ImTextureID)(intptr_t)texture->GetID(),
                            ImVec2(34.0f, 34.0f), ImVec2(0, 1), ImVec2(1, 0));
                        ImGui::SameLine();
                    }

                    const bool selected = currentPath == assetPath;
                    if (ImGui::Selectable(entry.path().filename().string().c_str(), selected,
                        ImGuiSelectableFlags_None, ImVec2(0.0f, 34.0f)))
                    {
                        image->SetTexturePath(assetPath);
                        ImGui::CloseCurrentPopup();
                    }
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("%s", assetPath.c_str());
                    ImGui::PopID();
                }
            }
            ImGui::EndCombo();
        }

        if (!currentPath.empty() && m_Renderer)
        {
            Texture2D* texture = m_Renderer->LoadTexture(currentPath);
            if (texture && texture->IsLoaded())
            {
                ImGui::TextDisabled("Preview");
                ImGui::Image((ImTextureID)(intptr_t)texture->GetID(),
                    ImVec2(96.0f, 96.0f), ImVec2(0, 1), ImVec2(1, 0));
            }
        }

    }

    if (UIButton* button = dynamic_cast<UIButton*>(&widget))
    {
        ImGui::Spacing();
        ImGui::SeparatorText("Button Style");
        ImGui::TextDisabled("Per-state appearance");

        Vec4 normal = button->GetNormalColor();
        if (ImGui::ColorEdit4("Normal", &normal.x))
            button->SetNormalColor(normal);

        Vec4 hoveredColor = button->GetHoveredColor();
        if (ImGui::ColorEdit4("Hovered", &hoveredColor.x))
            button->SetHoveredColor(hoveredColor);

        Vec4 pressedColor = button->GetPressedColor();
        if (ImGui::ColorEdit4("Pressed", &pressedColor.x))
            button->SetPressedColor(pressedColor);

        Vec4 disabledColor = button->GetDisabledColor();
        if (ImGui::ColorEdit4("Disabled", &disabledColor.x))
            button->SetDisabledColor(disabledColor);

        ImGui::Spacing();
        ImGui::TextDisabled("Runtime state");
        bool hovered = button->IsHovered();
        bool pressed = button->IsPressed();
        ImGui::BeginDisabled();
        ImGui::Checkbox("Hovered##RuntimeState", &hovered);
        ImGui::Checkbox("Pressed##RuntimeState", &pressed);
        ImGui::EndDisabled();
    }
}

void UIEditor::DrawToolbar(
    UICanvas& canvas)
{
    const float width = ImGui::GetContentRegionAvail().x;
    const bool compact = width < 900.0f;

    if (ImGui::Button("Open"))
        ImGui::OpenPopup("SelectUIAsset");
    ImGui::SameLine();
    if (ImGui::Button("Save"))
        UISerializer::Save(canvas, m_UIAssetPath);

    if (!compact)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("%s",
            std::filesystem::path(m_UIAssetPath).filename().string().c_str());
    }

    ImGui::SameLine();
    if (ImGui::Button("Panels"))
        ImGui::OpenPopup("UIPanelsPopup");

    if (ImGui::BeginPopup("UIPanelsPopup"))
    {
        ImGui::MenuItem("Palette", nullptr, &m_ShowPalette);
        ImGui::MenuItem("Hierarchy", nullptr, &m_ShowHierarchy);
        ImGui::MenuItem("Details", nullptr, &m_ShowDetails);
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("Edit"))
        ImGui::OpenPopup("UIEditPopup");

    if (ImGui::BeginPopup("UIEditPopup"))
    {
        if (ImGui::MenuItem("Undo", "Ctrl+Z", false, !m_UndoStack.empty())) Undo(canvas);
        if (ImGui::MenuItem("Redo", "Ctrl+Y", false, !m_RedoStack.empty())) Redo(canvas);
        ImGui::Separator();
        if (ImGui::MenuItem("Duplicate", "Ctrl+D")) { PushHistory(canvas); DuplicateSelected(canvas); }
        if (ImGui::MenuItem("Rename", "F2")) RenameSelected();
        if (ImGui::MenuItem("Delete", "Del")) { PushHistory(canvas); DeleteSelected(canvas); }
        ImGui::Separator();
        if (ImGui::MenuItem("Align Left")) AlignSelected(canvas,0);
        if (ImGui::MenuItem("Align Center X")) AlignSelected(canvas,1);
        if (ImGui::MenuItem("Align Right")) AlignSelected(canvas,2);
        if (ImGui::MenuItem("Align Top")) AlignSelected(canvas,3);
        if (ImGui::MenuItem("Align Center Y")) AlignSelected(canvas,4);
        if (ImGui::MenuItem("Align Bottom")) AlignSelected(canvas,5);
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("View"))
        ImGui::OpenPopup("UIViewPopup");

    if (ImGui::BeginPopup("UIViewPopup"))
    {
        ImGui::MenuItem("Grid", nullptr, &m_ShowGrid);
        ImGui::MenuItem("Snap", nullptr, &m_SnapToGrid);
        ImGui::MenuItem("Widget Bounds", nullptr, &m_ShowWidgetBounds);
        if (ImGui::MenuItem("Frame Canvas", "F")) ResetView();
        ImGui::Separator();
        ImGui::TextDisabled("Grid size");
        ImGui::SetNextItemWidth(110.0f);
        ImGui::DragFloat("##GridSizePopup", &m_GridSize, 1.0f, 1.0f, 200.0f, "%.0f");
        ImGui::EndPopup();
    }

    if (!compact)
    {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(95.0f);
        ImGui::SliderFloat("##Zoom", &m_Zoom, 0.25f, 2.0f, "%.2fx");
    }

    if (ImGui::BeginPopup("SelectUIAsset"))
    {
        ImGui::TextDisabled("UI ASSETS");
        ImGui::Separator();
        std::error_code ec;
        const std::filesystem::path root("Assets/UI");
        if (std::filesystem::exists(root, ec))
        {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(root, ec))
            {
                if (ec) break;
                if (!entry.is_regular_file()) continue;
                if (entry.path().extension() != ".ui") continue;

                const std::string path = entry.path().generic_string();
                if (ImGui::Selectable(path.c_str(), path == m_UIAssetPath))
                {
                    m_SelectedWidget = nullptr;
                    if (UISerializer::Load(canvas, path))
                        m_UIAssetPath = path;
                    ImGui::CloseCurrentPopup();
                }
            }
        }
        else
        {
            ImGui::TextDisabled("No Assets/UI folder found.");
        }
        ImGui::EndPopup();
    }
}

void UIEditor::DrawDesigner(
    UICanvas& canvas)
{
    ImGui::TextDisabled("%.0f x %.0f", canvas.GetSize().x, canvas.GetSize().y);
    ImGui::SameLine();
    ImGui::TextDisabled("  %.0f%%", m_DesignerScale * 100.0f);
    ImGui::Separator();

    const ImVec2 contentMin =
        ImGui::GetCursorScreenPos();

    const ImVec2 availableSize =
        ImGui::GetContentRegionAvail();

    if (availableSize.x <= 0.0f ||
        availableSize.y <= 0.0f)
    {
        return;
    }

    const Vec2 canvasSize =
        canvas.GetSize();

    /*
     * Calculate the scale exactly from the
     * available ImGui viewport, just like
     * the 3D editor viewport does.
     */
    const float scaleX =
        canvasSize.x > 0.0f
        ? availableSize.x /
        canvasSize.x
        : 1.0f;

    const float scaleY =
        canvasSize.y > 0.0f
        ? availableSize.y /
        canvasSize.y
        : 1.0f;

    /*
     * Preserve the canvas aspect ratio.
     */
    float scale =
        std::min(
            scaleX,
            scaleY
        );

    // Fit-to-window is the base scale. Manual zoom is relative to that
    // fit, so shrinking the editor never crops or distorts the canvas.
    scale *= m_Zoom;

    scale =
        std::max(
            0.05f,
            scale
        );

    m_DesignerScale = scale;

    /*
     * Actual displayed canvas size.
     */
    const ImVec2 canvasPixelSize(
        canvasSize.x * scale,
        canvasSize.y * scale
    );

    /*
     * Center the canvas in the designer.
     */
    const ImVec2 canvasPosition(
        contentMin.x +
        (availableSize.x - canvasPixelSize.x) * 0.5f +
        m_DesignerPan.x,
        contentMin.y +
        (availableSize.y - canvasPixelSize.y) * 0.5f +
        m_DesignerPan.y
    );

    m_DesignerCanvasPosition = canvasPosition;

    ImDrawList* drawList =
        ImGui::GetWindowDrawList();

    /*
     * Canvas background.
     */
    drawList->AddRectFilled(
        canvasPosition,
        ImVec2(
            canvasPosition.x +
            canvasPixelSize.x,

            canvasPosition.y +
            canvasPixelSize.y
        ),
        IM_COL32(
            35,
            38,
            45,
            255
        )
    );

    /*
     * Grid.
     */
    if (m_ShowGrid)
    {
        const float gridSpacing =
            m_GridSize * scale;

        if (gridSpacing >= 4.0f)
        {
            const float right =
                canvasPosition.x +
                canvasPixelSize.x;

            const float bottom =
                canvasPosition.y +
                canvasPixelSize.y;

            for (
                float x =
                canvasPosition.x;
                x <= right;
                x += gridSpacing)
            {
                drawList->AddLine(
                    ImVec2(
                        x,
                        canvasPosition.y
                    ),
                    ImVec2(
                        x,
                        bottom
                    ),
                    IM_COL32(
                        255,
                        255,
                        255,
                        20
                    )
                );
            }

            for (
                float y =
                canvasPosition.y;
                y <= bottom;
                y += gridSpacing)
            {
                drawList->AddLine(
                    ImVec2(
                        canvasPosition.x,
                        y
                    ),
                    ImVec2(
                        right,
                        y
                    ),
                    IM_COL32(
                        255,
                        255,
                        255,
                        20
                    )
                );
            }
        }
    }

    /*
     * Canvas border.
     */
    drawList->AddRect(
        canvasPosition,
        ImVec2(
            canvasPosition.x +
            canvasPixelSize.x,

            canvasPosition.y +
            canvasPixelSize.y
        ),
        IM_COL32(
            90,
            100,
            115,
            255
        ),
        0.0f,
        0,
        2.0f
    );

    /*
     * Convert mouse position to UI canvas
     * coordinates using the SAME scale that
     * was used to display the canvas.
     */
    const ImVec2 mouse =
        ImGui::GetMousePos();

    const bool mouseInsideCanvas =
        mouse.x >= canvasPosition.x &&
        mouse.x <=
        canvasPosition.x +
        canvasPixelSize.x &&
        mouse.y >= canvasPosition.y &&
        mouse.y <=
        canvasPosition.y +
        canvasPixelSize.y;

    /*
     * Draw the widget hierarchy.
     */
    if (canvas.GetRoot())
    {
        for (const auto& child :
            canvas.GetRoot()->GetChildren())
        {
            if (child)
            {
                UIRect rootRect;

                rootRect.x = 0.0f;
                rootRect.y = 0.0f;
                rootRect.width = canvasSize.x;
                rootRect.height = canvasSize.y;

                DrawWidget(
                    *child,
                    rootRect,
                    canvasPosition,
                    scale,
                    drawList
                );
            }
        }
    }

    // Register the designer before processing mouse input. ImGui only reports
    // clicks/drags against items submitted earlier in the frame.
    ImGui::SetCursorScreenPos(contentMin);
    ImGui::InvisibleButton("##UIDesignerDropTarget", availableSize,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

    // Pan the zoomed designer with middle mouse, or Space + left mouse.
    // Mouse-wheel zoom, centered on the cursor.
    if (ImGui::IsItemHovered() && !m_Panning && ImGui::GetIO().MouseWheel != 0.0f)
    {
        const float oldZoom = m_Zoom;
        const float newZoom = std::clamp(oldZoom * std::pow(1.12f, ImGui::GetIO().MouseWheel), 0.20f, 5.0f);
        if (std::abs(newZoom - oldZoom) > 0.0001f)
        {
            const float newScale = scale / oldZoom * newZoom;
            const ImVec2 center(contentMin.x + availableSize.x * 0.5f, contentMin.y + availableSize.y * 0.5f);
            const ImVec2 oldBase(center.x - canvasPixelSize.x * 0.5f, center.y - canvasPixelSize.y * 0.5f);
            const ImVec2 uiPoint((mouse.x - oldBase.x - m_DesignerPan.x) / scale,
                                 (mouse.y - oldBase.y - m_DesignerPan.y) / scale);
            const ImVec2 newSize(canvasSize.x * newScale, canvasSize.y * newScale);
            const ImVec2 newBase(center.x - newSize.x * 0.5f, center.y - newSize.y * 0.5f);
            m_Zoom = newZoom;
            m_DesignerPan = ImVec2(mouse.x - newBase.x - uiPoint.x * newScale,
                                   mouse.y - newBase.y - uiPoint.y * newScale);
        }
    }

    const bool panPressed = ImGui::IsMouseClicked(ImGuiMouseButton_Middle) ||
        (ImGui::IsKeyDown(ImGuiKey_Space) && ImGui::IsMouseClicked(ImGuiMouseButton_Left));
    if (ImGui::IsItemHovered() && panPressed)
    {
        m_Panning = true;
        m_PanStartMouse = ImGui::GetMousePos();
        m_PanStartOffset = m_DesignerPan;
    }
    if (m_Panning)
    {
        const ImVec2 current = ImGui::GetMousePos();
        m_DesignerPan = ImVec2(
            m_PanStartOffset.x + current.x - m_PanStartMouse.x,
            m_PanStartOffset.y + current.y - m_PanStartMouse.y);
        if ((!ImGui::IsMouseDown(ImGuiMouseButton_Middle) && !ImGui::IsKeyDown(ImGuiKey_Space)) ||
            (ImGui::IsKeyDown(ImGuiKey_Space) && !ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
             !ImGui::IsMouseDown(ImGuiMouseButton_Middle)))
            m_Panning = false;
    }

    if (mouseInsideCanvas && !m_Panning)
    {
        if (m_SelectedWidget && !m_Dragging && !m_Resizing &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            const UIRect selectedRect = GetAbsoluteRect(
                *m_SelectedWidget,
                UIRect{0.0f,0.0f,canvasSize.x,canvasSize.y});

            const int handle = GetResizeHandle(
                selectedRect, mouse, canvasPosition, scale);
            if (handle >= 0)
                BeginResize(*m_SelectedWidget, handle);
            else if (IsMouseInsideRect(
                selectedRect, mouse, canvasPosition, scale))
                BeginDrag(*m_SelectedWidget);
        }

        if (m_Dragging && m_SelectedWidget)
            UpdateDrag(*m_SelectedWidget);
        if (m_Resizing && m_SelectedWidget)
            UpdateResize(*m_SelectedWidget);
    }

    // Always terminate an active gesture, even if the pointer was released
    // just outside the canvas.
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        m_Dragging=false;
        m_Resizing=false;
        m_ResizeHandle=-1;
    }

    if (mouseInsideCanvas && ImGui::IsItemClicked(ImGuiMouseButton_Right))
        ImGui::OpenPopup("DesignerCreatePopup");
    if (ImGui::BeginPopup("DesignerCreatePopup"))
    {
        ImGui::TextDisabled("CREATE WIDGET");
        if(ImGui::MenuItem("Panel")) { PushHistory(canvas); AddWidget(canvas,UIWidgetType::Panel); }
        if(ImGui::MenuItem("Text")) { PushHistory(canvas); AddWidget(canvas,UIWidgetType::Text); }
        if(ImGui::MenuItem("Image")) { PushHistory(canvas); AddWidget(canvas,UIWidgetType::Image); }
        if(ImGui::MenuItem("Button")) { PushHistory(canvas); AddWidget(canvas,UIWidgetType::Button); }
        ImGui::EndPopup();
    }
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("UI_PALETTE_WIDGET"))
        {
            if (payload->DataSize == sizeof(int))
            {
                const int value = *static_cast<const int*>(payload->Data);
                if (value >= static_cast<int>(UIWidgetType::Panel) &&
                    value <= static_cast<int>(UIWidgetType::Button))
                {
                    AddWidget(canvas, static_cast<UIWidgetType>(value));
                    if (m_SelectedWidget)
                    {
                        Vec2 position(
                            (ImGui::GetMousePos().x - canvasPosition.x) / scale,
                            (ImGui::GetMousePos().y - canvasPosition.y) / scale);
                        if (m_SnapToGrid) { position.x = SnapValue(position.x); position.y = SnapValue(position.y); }
                        m_SelectedWidget->SetPosition(position);
                    }
                }
            }
        }
        ImGui::EndDragDropTarget();
    }
}

void UIEditor::DrawWidget(
    UIWidget& widget,
    const UIRect& parentRect,
    const ImVec2& canvasPosition,
    float scale,
    ImDrawList* drawList)
{
    if (!widget.IsVisible())
    {
        return;
    }

    const UIRect rect =
        UILayout::Calculate(
            widget,
            parentRect
        );

    const ImVec2 min(
        canvasPosition.x +
        rect.x * scale,

        canvasPosition.y +
        rect.y * scale
    );

    const ImVec2 max(
        min.x +
        rect.width * scale,

        min.y +
        rect.height * scale
    );

    Vec4 color =
        widget.GetColor();

    ImU32 fillColor =
        IM_COL32(
            static_cast<int>(
                std::clamp(
                    color.x,
                    0.0f,
                    1.0f
                ) * 255.0f
                ),

            static_cast<int>(
                std::clamp(
                    color.y,
                    0.0f,
                    1.0f
                ) * 255.0f
                ),

            static_cast<int>(
                std::clamp(
                    color.z,
                    0.0f,
                    1.0f
                ) * 255.0f
                ),

            static_cast<int>(
                std::clamp(
                    color.w,
                    0.0f,
                    1.0f
                ) * 255.0f
                )
        );

    if (widget.GetType() ==
        UIWidgetType::Text)
    {
        if (UIText* text =
            dynamic_cast<UIText*>(
                &widget))
        {
            const char* textValue =
                text->GetText().c_str();

            const float fontSize =
                text->GetFontSize() *
                scale;

            // Use the same Inter face as the runtime UI. The editor's first
            // regular font is loaded from Assets/Fonts/InterVariable.ttf;
            // avoid whichever temporary/icon font happens to be active while
            // the Widget Blueprint window is drawing.
            ImFont* previewFont = ImGui::GetIO().Fonts->Fonts.empty()
                ? ImGui::GetFont()
                : ImGui::GetIO().Fonts->Fonts.front();
            const float previewSize = std::max(1.0f, fontSize);

            drawList->AddText(
                previewFont,
                previewSize,
                min,
                fillColor,
                textValue
            );
        }
    }
    else if (widget.GetType() == UIWidgetType::Image)
    {
        const UIImage* image = dynamic_cast<const UIImage*>(&widget);
        Texture2D* texture = nullptr;
        if (image && m_Renderer && !image->GetTexturePath().empty())
            texture = m_Renderer->LoadTexture(image->GetTexturePath());

        if (texture && texture->IsLoaded())
        {
            drawList->AddImage(
                (ImTextureID)(intptr_t)texture->GetID(),
                min,
                max,
                ImVec2(0.0f, 1.0f),
                ImVec2(1.0f, 0.0f),
                fillColor);
        }
        else
        {
            drawList->AddRectFilled(min, max, IM_COL32(45, 47, 52, 255), 2.0f);
            drawList->AddRect(min, max, IM_COL32(100, 105, 115, 255), 2.0f);
            drawList->AddText(ImVec2(min.x + 8.0f, min.y + 8.0f),
                IM_COL32(160, 165, 175, 255), "No Image");
        }
    }
    else
    {
        drawList->AddRectFilled(
            min,
            max,
            fillColor,
            4.0f
        );

        drawList->AddRect(
            min,
            max,
            IM_COL32(
                255,
                255,
                255,
                70
            ),
            4.0f
        );

        // Buttons are visual containers. Their widget name is editor metadata,
        // not visible UI text. Add a UIText child when the button needs a label.

    }

    if (m_ShowWidgetBounds && m_SelectedWidget != &widget)
        drawList->AddRect(min,max,IM_COL32(110,120,135,80),0.0f,0,1.0f);

    /*
     * Selection outline and resize handles.
     */
    if (m_SelectedWidget ==
        &widget)
    {
        drawList->AddRect(
            min,
            max,
            IM_COL32(
                77,
                163,
                255,
                255
            ),
            0.0f,
            0,
            2.0f
        );

        constexpr float handleSize =
            6.0f;

        const ImVec2 handles[] =
        {
            ImVec2(
                min.x,
                min.y
            ),

            ImVec2(
                max.x,
                min.y
            ),

            ImVec2(
                min.x,
                max.y
            ),

            ImVec2(
                max.x,
                max.y
            )
        };

        for (const ImVec2& handle :
            handles)
        {
            drawList->AddRectFilled(
                ImVec2(
                    handle.x -
                    handleSize,

                    handle.y -
                    handleSize
                ),

                ImVec2(
                    handle.x +
                    handleSize,

                    handle.y +
                    handleSize
                ),

                IM_COL32(
                    77,
                    163,
                    255,
                    255
                )
            );
        }
    }

    for (const auto& child :
        widget.GetChildren())
    {
        if (child)
        {
            DrawWidget(
                *child,
                rect,
                canvasPosition,
                scale,
                drawList
            );
        }
    }
}

void UIEditor::SelectWidget(
    UIWidget* widget)
{
    m_SelectedWidget =
        widget;

    m_Dragging =
        false;

    m_Resizing =
        false;

    m_ResizeHandle =
        -1;
}

void UIEditor::BeginDrag(
    UIWidget& widget)
{
    m_Dragging =
        true;

    m_Resizing =
        false;

    m_DragStartMouse =
        Vec2(
            ImGui::GetMousePos().x,
            ImGui::GetMousePos().y
        );

    m_DragStartPosition =
        widget.GetPosition();
}

void UIEditor::UpdateDrag(
    UIWidget& widget)
{
    const float scale = std::max(0.05f, m_DesignerScale);

    const ImVec2 mouse =
        ImGui::GetMousePos();

    const float deltaX =
        (mouse.x -
            m_DragStartMouse.x) /
        scale;

    const float deltaY =
        (mouse.y -
            m_DragStartMouse.y) /
        scale;

    Vec2 position =
        m_DragStartPosition;

    position.x += deltaX;
    position.y += deltaY;

    if (m_SnapToGrid)
    {
        position.x =
            SnapValue(
                position.x
            );

        position.y =
            SnapValue(
                position.y
            );
    }

    widget.SetPosition(
        position
    );
}

void UIEditor::BeginResize(
    UIWidget& widget,
    int handle)
{
    m_Resizing =
        true;

    m_Dragging =
        false;

    m_ResizeHandle =
        handle;

    m_DragStartMouse =
        Vec2(
            ImGui::GetMousePos().x,
            ImGui::GetMousePos().y
        );

    m_DragStartPosition =
        widget.GetPosition();

    m_DragStartSize =
        widget.GetSize();
}

void UIEditor::UpdateResize(
    UIWidget& widget)
{
    const float scale = std::max(0.05f, m_DesignerScale);

    const ImVec2 mouse =
        ImGui::GetMousePos();

    const float deltaX =
        (mouse.x -
            m_DragStartMouse.x) /
        scale;

    const float deltaY =
        (mouse.y -
            m_DragStartMouse.y) /
        scale;

    Vec2 position =
        m_DragStartPosition;

    Vec2 size =
        m_DragStartSize;

    switch (m_ResizeHandle)
    {
    case 0:
        position.x += deltaX;
        position.y += deltaY;

        size.x -= deltaX;
        size.y -= deltaY;
        break;

    case 1:
        position.y += deltaY;

        size.x += deltaX;
        size.y -= deltaY;
        break;

    case 2:
        position.x += deltaX;

        size.x -= deltaX;
        size.y += deltaY;
        break;

    case 3:
        size.x += deltaX;
        size.y += deltaY;
        break;
    }

    constexpr float minimumSize =
        1.0f;

    if (size.x < minimumSize)
    {
        size.x =
            minimumSize;
    }

    if (size.y < minimumSize)
    {
        size.y =
            minimumSize;
    }

    if (m_SnapToGrid)
    {
        position.x =
            SnapValue(
                position.x
            );

        position.y =
            SnapValue(
                position.y
            );

        size.x =
            SnapValue(
                size.x
            );

        size.y =
            SnapValue(
                size.y
            );
    }

    widget.SetPosition(
        position
    );

    widget.SetSize(
        size
    );
}

int UIEditor::GetResizeHandle(
    const UIRect& rect,
    const ImVec2& mouse,
    const ImVec2& canvasPosition,
    float scale) const
{
    const ImVec2 min(
        canvasPosition.x +
        rect.x * scale,

        canvasPosition.y +
        rect.y * scale
    );

    const ImVec2 max(
        min.x +
        rect.width * scale,

        min.y +
        rect.height * scale
    );

    constexpr float handleRadius =
        10.0f;

    const ImVec2 handles[] =
    {
        ImVec2(
            min.x,
            min.y
        ),

        ImVec2(
            max.x,
            min.y
        ),

        ImVec2(
            min.x,
            max.y
        ),

        ImVec2(
            max.x,
            max.y
        )
    };

    for (int i = 0; i < 4; ++i)
    {
        const float dx =
            mouse.x -
            handles[i].x;

        const float dy =
            mouse.y -
            handles[i].y;

        if (dx * dx +
            dy * dy <=
            handleRadius *
            handleRadius)
        {
            return i;
        }
    }

    return -1;
}

bool UIEditor::IsMouseInsideRect(
    const UIRect& rect,
    const ImVec2& mouse,
    const ImVec2& canvasPosition,
    float scale) const
{
    const float left =
        canvasPosition.x +
        rect.x * scale;

    const float top =
        canvasPosition.y +
        rect.y * scale;

    const float right =
        left +
        rect.width * scale;

    const float bottom =
        top +
        rect.height * scale;

    return mouse.x >= left &&
        mouse.x <= right &&
        mouse.y >= top &&
        mouse.y <= bottom;
}

float UIEditor::SnapValue(
    float value) const
{
    if (m_GridSize <= 0.0f)
    {
        return value;
    }

    return std::round(
        value /
        m_GridSize
    ) *
        m_GridSize;
}

void UIEditor::DeleteSelected(
    UICanvas& canvas)
{
    if (!m_SelectedWidget)
    {
        return;
    }

    if (m_SelectedWidget ==
        canvas.GetRoot())
    {
        return;
    }

    UIWidget* parent =
        m_SelectedWidget->GetParent();

    if (!parent)
    {
        return;
    }

    parent->RemoveChild(
        m_SelectedWidget
    );

    m_SelectedWidget =
        nullptr;

    m_Dragging =
        false;

    m_Resizing =
        false;

    m_ResizeHandle =
        -1;
}

void UIEditor::DuplicateSelected(UICanvas& canvas)
{
    if(!m_SelectedWidget) return;
    UIWidget* parent=m_SelectedWidget->GetParent();
    if(!parent) parent=canvas.GetRoot();
    if(!parent) return;
    std::unique_ptr<UIWidget> copy=CloneWidget(*m_SelectedWidget);
    if(!copy) return;
    copy->SetName(m_SelectedWidget->GetName()+" Copy");
    Vec2 position=copy->GetPosition();
    position.x+=m_GridSize; position.y+=m_GridSize; copy->SetPosition(position);
    UIWidget* added=parent->AddChild(std::move(copy));
    SelectWidget(added);
}

void UIEditor::AddWidget(
    UICanvas& canvas,
    UIWidgetType type)
{
    UIWidget* parent =
        m_SelectedWidget;

    if (!parent ||
        parent->GetType() ==
        UIWidgetType::Text ||
        parent->GetType() ==
        UIWidgetType::Image ||
        parent->GetType() ==
        UIWidgetType::Button)
    {
        parent =
            canvas.GetRoot();
    }

    if (!parent)
    {
        return;
    }

    std::unique_ptr<UIWidget> widget =
        UIWidgetFactory::Create(
            type
        );

    if (!widget)
    {
        return;
    }

    Vec2 position(
        50.0f,
        50.0f
    );

    widget->SetPosition(
        position
    );

    widget->SetSize(
        Vec2(
            200.0f,
            100.0f
        )
    );

    UIWidget* added =
        parent->AddChild(
            std::move(widget)
        );

    SelectWidget(
        added
    );
}

void UIEditor::RenameSelected()
{
    if (!m_SelectedWidget)
    {
        return;
    }

    ImGui::OpenPopup(
        "RenameWidget"
    );
}

void UIEditor::ResetView()
{
    m_Zoom = 1.0f;
    m_DesignerPan = ImVec2(0.0f, 0.0f);
    m_Panning = false;
}

UIRect UIEditor::GetAbsoluteRect(
    const UIWidget& widget,
    const UIRect& canvasRect) const
{
    const UIWidget* parent = widget.GetParent();
    if (parent == nullptr || parent->GetParent() == nullptr)
        return UILayout::Calculate(widget, canvasRect);

    const UIRect parentRect = GetAbsoluteRect(*parent, canvasRect);
    return UILayout::Calculate(widget, parentRect);
}

std::unique_ptr<UIWidget> UIEditor::CloneWidget(const UIWidget& source) const
{
    std::unique_ptr<UIWidget> copy=UIWidgetFactory::Create(source.GetType());
    if(!copy) return nullptr;
    copy->SetName(source.GetName()); copy->SetPosition(source.GetPosition()); copy->SetSize(source.GetSize());
    copy->SetAnchors(source.GetAnchorMinimum(),source.GetAnchorMaximum()); copy->SetPivot(source.GetPivot());
    copy->SetColor(source.GetColor()); copy->SetVisible(source.IsVisible()); copy->SetEnabled(source.IsEnabled());
    copy->SetHitTestVisible(source.IsHitTestVisible()); copy->SetZOrder(source.GetZOrder());
    if(auto* a=dynamic_cast<const UIText*>(&source)) if(auto* b=dynamic_cast<UIText*>(copy.get())) { b->SetText(a->GetText()); b->SetFontSize(a->GetFontSize()); }
    if(auto* a=dynamic_cast<const UIImage*>(&source)) if(auto* b=dynamic_cast<UIImage*>(copy.get())) b->SetTexturePath(a->GetTexturePath());
    if(auto* a=dynamic_cast<const UIButton*>(&source)) if(auto* b=dynamic_cast<UIButton*>(copy.get())) {
        b->SetNormalColor(a->GetNormalColor()); b->SetHoveredColor(a->GetHoveredColor());
        b->SetPressedColor(a->GetPressedColor()); b->SetDisabledColor(a->GetDisabledColor());
    }
    for(const auto& child:source.GetChildren()) if(child) copy->AddChild(CloneWidget(*child));
    return copy;
}

void UIEditor::NudgeSelected(float x,float y)
{
    if(!m_SelectedWidget) return;
    Vec2 p=m_SelectedWidget->GetPosition(); p.x+=x; p.y+=y; m_SelectedWidget->SetPosition(p);
}

void UIEditor::AlignSelected(UICanvas& canvas,int mode)
{
    if(!m_SelectedWidget || m_SelectedWidget==canvas.GetRoot()) return;
    UIWidget* parent=m_SelectedWidget->GetParent();
    Vec2 bounds=parent && parent!=canvas.GetRoot()?parent->GetSize():canvas.GetSize();
    Vec2 p=m_SelectedWidget->GetPosition(), size=m_SelectedWidget->GetSize();
    if(mode==0)p.x=0; else if(mode==1)p.x=(bounds.x-size.x)*.5f; else if(mode==2)p.x=bounds.x-size.x;
    else if(mode==3)p.y=0; else if(mode==4)p.y=(bounds.y-size.y)*.5f; else if(mode==5)p.y=bounds.y-size.y;
    m_SelectedWidget->SetPosition(p);
}

std::string UIEditor::CaptureCanvas(UICanvas& canvas) const
{
    if(m_UIAssetPath.empty()) return {};
    const std::filesystem::path temp=std::filesystem::temp_directory_path()/"vortek_ui_editor_history.ui";
    if(!UISerializer::Save(canvas,temp.string())) return {};
    std::ifstream in(temp,std::ios::binary); return std::string((std::istreambuf_iterator<char>(in)),{});
}

bool UIEditor::RestoreCanvas(UICanvas& canvas,const std::string& snapshot)
{
    if(snapshot.empty()) return false;
    const std::filesystem::path temp=std::filesystem::temp_directory_path()/"vortek_ui_editor_history.ui";
    { std::ofstream out(temp,std::ios::binary|std::ios::trunc); out<<snapshot; }
    m_SelectedWidget=nullptr; m_Dragging=false; m_Resizing=false;
    return UISerializer::Load(canvas,temp.string());
}

void UIEditor::PushHistory(UICanvas& canvas)
{
    std::string snapshot=CaptureCanvas(canvas); if(snapshot.empty()) return;
    if(m_UndoStack.size()>=MaxHistory) m_UndoStack.erase(m_UndoStack.begin());
    m_UndoStack.push_back(std::move(snapshot)); m_RedoStack.clear();
}

void UIEditor::Undo(UICanvas& canvas)
{
    if(m_UndoStack.empty()) return;
    std::string current=CaptureCanvas(canvas); std::string previous=std::move(m_UndoStack.back()); m_UndoStack.pop_back();
    if(!current.empty()) m_RedoStack.push_back(std::move(current)); RestoreCanvas(canvas,previous);
}

void UIEditor::Redo(UICanvas& canvas)
{
    if(m_RedoStack.empty()) return;
    std::string current=CaptureCanvas(canvas); std::string next=std::move(m_RedoStack.back()); m_RedoStack.pop_back();
    if(!current.empty()) m_UndoStack.push_back(std::move(current)); RestoreCanvas(canvas,next);
}
