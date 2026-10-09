#include "UIEditor.h"

#include "../../UI/UIWidget.h"
#include "../../UI/UIWidgetFactory.h"
#include "../../UI/UIPanel.h"
#include "../../UI/UIText.h"
#include "../../UI/UIImage.h"
#include "../../UI/UIButton.h"
#include "../../UI/UITextInput.h"
#include "../../UI/UISlider.h"
#include "../../UI/UIProgressBar.h"
#include "../../UI/UIScrollBox.h"
#include "../../UI/UIUserWidget.h"
#include "../../UI/UISerializer.h"
#include "../../Graphics/Renderer.h"
#include "../../Graphics/Texture2D.h"

#include <imgui.h>
#include <imgui_internal.h>

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
#include <cfloat>
#include <fstream>
#include <sstream>
#include <chrono>

namespace
{
    bool IsLinkedWidgetChild(const UIWidget* widget)
    {
        for (const UIWidget* parent = widget ? widget->GetParent() : nullptr;
             parent; parent = parent->GetParent())
            if (parent->GetType() == UIWidgetType::UserWidget) return true;
        return false;
    }
}

bool UIEditor::OpenAsset(UICanvas& canvas, const std::string& path)
{
    m_SelectedWidget = nullptr;
    m_Visible = true;

    if (!UISerializer::Load(canvas, path))
        return false;

    m_UIAssetPath = std::filesystem::absolute(std::filesystem::path(path)).lexically_normal().generic_string();
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

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    // The main menu is already excluded from WorkPos/WorkSize. Reserve the
    // document strip locally without mutating ImGui's global viewport state.
    const float documentBarHeight = 34.0f;
    const ImVec2 workspacePos(
        viewport->WorkPos.x,
        viewport->WorkPos.y + documentBarHeight);
    const ImVec2 workspaceSize(
        viewport->WorkSize.x,
        std::max(1.0f, viewport->WorkSize.y - documentBarHeight));

    const std::string hostName =
        std::string("##UIWorkspace_") + m_UIAssetPath;
    ImGui::SetNextWindowPos(workspacePos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(workspaceSize, ImGuiCond_Always);
    if (m_FocusRequested)
    {
        ImGui::SetNextWindowFocus();
        m_FocusRequested = false;
    }

    const ImGuiWindowFlags hostFlags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin(hostName.c_str(), nullptr, hostFlags);
    ImGui::PopStyleVar();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(24, 26, 29, 255));
    ImGui::BeginChild("WidgetToolbar", ImVec2(0.0f, 42.0f), false);
    ImGui::SetCursorPos(ImVec2(8.0f, 7.0f));
    DrawToolbar(canvas);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::PushID(m_UIAssetPath.c_str());
    const ImGuiID dockspaceId = ImGui::GetID("WidgetBlueprintDockSpace");

    // Recreate the provided default Widget Blueprint layout only when this
    // document has no saved docking state. Saved imgui.ini layouts still win.
    if (ImGui::DockBuilderGetNode(dockspaceId) == nullptr)
    {
        const ImVec2 dockSize(
            workspaceSize.x,
            std::max(1.0f, workspaceSize.y - 42.0f));

        ImGui::DockBuilderRemoveNode(dockspaceId);
        ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceId, dockSize);

        ImGuiID center = dockspaceId;
        ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.143f, nullptr, &center);
        ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.18f, nullptr, &center);
        ImGuiID hierarchy = ImGui::DockBuilderSplitNode(left, ImGuiDir_Down, 0.392f, nullptr, &left);

        ImGui::DockBuilderDockWindow("Palette##UIEditor", left);
        ImGui::DockBuilderDockWindow("Hierarchy##UIEditor", hierarchy);
        ImGui::DockBuilderDockWindow("Designer##UIEditor", center);
        ImGui::DockBuilderDockWindow("Details##UIEditor", right);
        ImGui::DockBuilderFinish(dockspaceId);
    }

    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

    if (m_ShowPalette)
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(29, 31, 35, 255));
        ImGui::Begin("Palette##UIEditor", &m_ShowPalette);
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

            ImGui::PushID(static_cast<int>(type));
            if (ImGui::Selectable(label))
                AddWidget(canvas, type);
            if (!IsLinkedWidgetChild(&widget) && ImGui::BeginDragDropSource())
            {
                const int payloadType = static_cast<int>(type);
                ImGui::SetDragDropPayload("UI_PALETTE_WIDGET", &payloadType, sizeof(payloadType));
                ImGui::Text("Add %s", label);
                ImGui::EndDragDropSource();
            }
            ImGui::PopID();
        };
        if (ImGui::CollapsingHeader("Common##PaletteCommon", ImGuiTreeNodeFlags_DefaultOpen))
        {
            paletteItem("Text", UIWidgetType::Text);
            paletteItem("Image", UIWidgetType::Image);
            paletteItem("Button", UIWidgetType::Button);
            paletteItem("Progress Bar", UIWidgetType::ProgressBar);
        }
        if (ImGui::CollapsingHeader("Input##PaletteInput", ImGuiTreeNodeFlags_DefaultOpen))
        {
            paletteItem("Text Input", UIWidgetType::TextInput);
            paletteItem("Slider", UIWidgetType::Slider);
        }
        if (ImGui::CollapsingHeader("Panel##PalettePanel", ImGuiTreeNodeFlags_DefaultOpen))
        {
            paletteItem("Panel", UIWidgetType::Panel);
            paletteItem("Scroll Box", UIWidgetType::ScrollBox);
            paletteItem("User Widget", UIWidgetType::UserWidget);
        }
        ImGui::Spacing();
        ImGui::TextDisabled("Click to add, or drag into the Designer");
        ImGui::End();
        ImGui::PopStyleColor();
    }

    if (m_ShowHierarchy)
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(24, 26, 30, 255));
        ImGui::Begin("Hierarchy##UIEditor", &m_ShowHierarchy);
        ImGui::TextDisabled("HIERARCHY");
        ImGui::Separator();
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##UIHierarchySearch", "Search hierarchy...", m_HierarchySearch, sizeof(m_HierarchySearch));
        std::string hierarchyQuery=m_HierarchySearch;
        std::transform(hierarchyQuery.begin(),hierarchyQuery.end(),hierarchyQuery.begin(),
            [](unsigned char c){return static_cast<char>(std::tolower(c));});
        if (canvas.GetRoot()) DrawHierarchy(canvas, *canvas.GetRoot(), hierarchyQuery);
        ImGui::End();
        ImGui::PopStyleColor();
    }

    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(17, 18, 21, 255));
    ImGui::SetNextWindowDockID(dockspaceId, ImGuiCond_FirstUseEver);
    ImGui::Begin("Designer##UIEditor");
    DrawDesigner(canvas);
    ImGui::End();
    ImGui::PopStyleColor();

    if (m_ShowDetails)
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(29, 31, 35, 255));
        ImGui::Begin("Details##UIEditor", &m_ShowDetails);
        if (m_SelectedWidget)
            DrawInspector(*m_SelectedWidget);
        else
            ImGui::TextDisabled("Select a widget to edit its properties.");
        ImGui::End();
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

    if (m_RenameRequested)
    {
        ImGui::OpenPopup("Rename Widget");
        m_RenameRequested = false;
    }
    if (ImGui::BeginPopupModal("Rename Widget", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::SetNextItemWidth(280.0f);
        const bool enter = ImGui::InputText("##RenameWidgetName", m_RenameBuffer, sizeof(m_RenameBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
        if ((enter || ImGui::Button("Rename")) && m_SelectedWidget && m_RenameBuffer[0] != '\0')
        {
            m_SelectedWidget->SetName(m_RenameBuffer);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::PopID();
    ImGui::End();
}

void UIEditor::DrawHierarchy(
    UICanvas& canvas,
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
        widget.GetType()==UIWidgetType::Image ? "[Image]" :
        widget.GetType()==UIWidgetType::Button ? "[Button]" :
        widget.GetType()==UIWidgetType::TextInput ? "[TextInput]" :
        widget.GetType()==UIWidgetType::Slider ? "[Slider]" :
        widget.GetType()==UIWidgetType::ScrollBox ? "[ScrollBox]" :
        widget.GetType()==UIWidgetType::UserWidget ? "[UserWidget]" : "[Progress]";
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

    if (!IsLinkedWidgetChild(&widget) && ImGui::BeginPopupContextItem("##WidgetContext"))
    {
        if (m_SelectedWidget != &widget)
            SelectWidget(&widget);
        if (ImGui::MenuItem("Rename", "F2"))
            RenameSelected();
        if (ImGui::MenuItem("Duplicate", "Ctrl+D"))
        {
            PushHistory(canvas);
            DuplicateSelected(canvas);
        }
        if (ImGui::MenuItem("Delete", "Del", false, widget.GetParent() != nullptr))
        {
            PushHistory(canvas);
            DeleteSelected(canvas);
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginDragDropSource())
    {
        UIWidget* draggedWidget = &widget;
        ImGui::SetDragDropPayload("UI_WIDGET_REPARENT", &draggedWidget, sizeof(draggedWidget));
        ImGui::Text("Move %s", label.c_str());
        ImGui::EndDragDropSource();
    }

    if (!IsLinkedWidgetChild(&widget) && widget.GetType() != UIWidgetType::UserWidget && ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("UI_WIDGET_REPARENT"))
        {
            if (payload->DataSize == sizeof(UIWidget*))
            {
                UIWidget* draggedWidget = *static_cast<UIWidget* const*>(payload->Data);
                const bool targetCanContainChildren =
                    widget.GetType() == UIWidgetType::Panel ||
                    widget.GetType() == UIWidgetType::ScrollBox;
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
                    canvas,
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
    auto typeName=[](UIWidgetType type)->const char*
    {
        switch(type)
        {
        case UIWidgetType::Panel: return "Panel";
        case UIWidgetType::Text: return "Text";
        case UIWidgetType::Image: return "Image";
        case UIWidgetType::Button: return "Button";
        case UIWidgetType::TextInput: return "Text Input";
        case UIWidgetType::Slider: return "Slider";
        case UIWidgetType::ProgressBar: return "Progress Bar";
        case UIWidgetType::ScrollBox: return "Scroll Box";
        case UIWidgetType::UserWidget: return "User Widget";
        }
        return "Widget";
    };

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 6.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, 9.0f);

    ImGui::Dummy(ImVec2(0.0f,2.0f));
    ImGui::TextColored(ImVec4(0.55f,0.72f,0.95f,1.0f), "%s", typeName(widget.GetType()));
    if (widget.GetParent())
    {
        ImGui::SameLine();
        ImGui::TextDisabled("  in %s", widget.GetParent()->GetName().c_str());
    }

    char nameBuffer[256]{};
    std::snprintf(nameBuffer,sizeof(nameBuffer),"%s",widget.GetName().c_str());
    ImGui::SetNextItemWidth(-1.0f);
    if(ImGui::InputText("##WidgetName",nameBuffer,sizeof(nameBuffer)))
        widget.SetName(nameBuffer);

    ImGui::Dummy(ImVec2(0.0f,2.0f));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f,2.0f));

    const float filterButtonWidth=48.0f;
    ImGui::SetNextItemWidth(std::max(80.0f,ImGui::GetContentRegionAvail().x-filterButtonWidth-4.0f));
    ImGui::InputTextWithHint("##DetailsSearch","Search Details",m_DetailsSearch,sizeof(m_DetailsSearch));
    ImGui::SameLine();
    if(ImGui::Button("View",ImVec2(filterButtonWidth,0)))
        ImGui::OpenPopup("##DetailsView");
    if(ImGui::BeginPopup("##DetailsView"))
    {
        ImGui::MenuItem("Show Advanced",nullptr,&m_DetailsShowAdvanced);
        ImGui::Separator();
        if(ImGui::MenuItem("Expand All")) m_DetailsExpandRequest=1;
        if(ImGui::MenuItem("Collapse All")) m_DetailsExpandRequest=-1;
        if(ImGui::MenuItem("Clear Search")) m_DetailsSearch[0]='\0';
        ImGui::EndPopup();
    }

    std::string query=m_DetailsSearch;
    std::transform(query.begin(),query.end(),query.begin(),[](unsigned char ch){return static_cast<char>(std::tolower(ch));});
    auto contains=[&](const std::string& value)
    {
        if(query.empty()) return true;
        std::string lower=value;
        std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char ch){return static_cast<char>(std::tolower(ch));});
        return lower.find(query)!=std::string::npos;
    };
    auto categoryVisible=[&](const char* category,const char* keywords)
    {
        return query.empty() || contains(category) || contains(keywords ? keywords : "");
    };
    auto propertyVisible=[&](const char* category,const char* property,const char* aliases="")
    {
        if(query.empty()) return true;
        return contains(category) || contains(property) || contains(aliases);
    };

    auto category=[&](const char* name,const char* keywords,bool defaultOpen,auto&& body)
    {
        if(!categoryVisible(name,keywords)) return;
        ImGui::PushID(name);
        ImGui::Dummy(ImVec2(0.0f,3.0f));
        if(!query.empty() || m_DetailsExpandRequest!=0)
            ImGui::SetNextItemOpen(!query.empty() || m_DetailsExpandRequest>0,ImGuiCond_Always);
        ImGuiTreeNodeFlags flags=defaultOpen?ImGuiTreeNodeFlags_DefaultOpen:ImGuiTreeNodeFlags_None;
        ImGui::PushStyleColor(ImGuiCol_Header,ImVec4(0.15f,0.17f,0.20f,1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered,ImVec4(0.19f,0.22f,0.26f,1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,ImVec4(0.22f,0.25f,0.30f,1.0f));
        if(ImGui::CollapsingHeader(name,flags))
        {
            ImGui::Dummy(ImVec2(0.0f,2.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding,ImVec2(7.0f,5.0f));
            if(ImGui::BeginTable("##Properties",2,ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_BordersInnerH|ImGuiTableFlags_RowBg))
            {
                ImGui::TableSetupColumn("Property",ImGuiTableColumnFlags_WidthFixed,std::clamp(ImGui::GetContentRegionAvail().x*0.42f,86.0f,150.0f));
                ImGui::TableSetupColumn("Value",ImGuiTableColumnFlags_WidthStretch);
                auto row=[&](const char* label,const char* aliases,auto&& control)
                {
                    if(!propertyVisible(name,label,aliases?aliases:"")) return;
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::AlignTextToFramePadding();
                    ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(0.78f,0.80f,0.84f,1.0f));
                    ImGui::TextUnformatted(label);
                    ImGui::PopStyleColor();
                    ImGui::TableSetColumnIndex(1);
                    ImGui::PushID(label);
                    ImGui::SetNextItemWidth(-1.0f);
                    control();
                    ImGui::PopID();
                };
                body(row);
                ImGui::EndTable();
            }
            ImGui::PopStyleVar();
            ImGui::Dummy(ImVec2(0.0f,1.0f));
        }
        ImGui::PopStyleColor(3);
        ImGui::PopID();
    };

    auto findAssetRoot=[&]()
    {
        std::filesystem::path assetRoot("Assets");
        const std::filesystem::path uiPath(m_UIAssetPath);
        for(std::filesystem::path parent=uiPath.parent_path();!parent.empty();parent=parent.parent_path())
        {
            if(parent.filename()=="Assets"){assetRoot=parent;break;}
            if(parent==parent.root_path())break;
        }
        return assetRoot;
    };

    auto texturePicker=[&](const std::string& current,const std::function<void(const std::string&)>& setter)
    {
        const std::string preview=current.empty()?"None":std::filesystem::path(current).filename().string();
        if(ImGui::BeginCombo("##Value",preview.c_str()))
        {
            static char textureSearch[96]{};
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint("##TextureSearch","Search textures...",textureSearch,sizeof(textureSearch));
            if(ImGui::Selectable("None",current.empty())){setter("");ImGui::CloseCurrentPopup();}
            std::string needle=textureSearch;
            std::transform(needle.begin(),needle.end(),needle.begin(),[](unsigned char ch){return static_cast<char>(std::tolower(ch));});
            std::error_code ec;const auto assetRoot=findAssetRoot();
            if(std::filesystem::exists(assetRoot,ec))
            {
                for(const auto& entry:std::filesystem::recursive_directory_iterator(assetRoot,ec))
                {
                    if(ec)break;if(!entry.is_regular_file())continue;
                    std::string ext=entry.path().extension().string();
                    std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char ch){return static_cast<char>(std::tolower(ch));});
                    if(ext!=".png"&&ext!=".jpg"&&ext!=".jpeg"&&ext!=".bmp"&&ext!=".tga")continue;
                    const auto rel=std::filesystem::path("Assets")/std::filesystem::relative(entry.path(),assetRoot,ec);if(ec)continue;
                    const std::string path=rel.lexically_normal().generic_string();
                    std::string searchable=path;std::transform(searchable.begin(),searchable.end(),searchable.begin(),[](unsigned char ch){return static_cast<char>(std::tolower(ch));});
                    if(!needle.empty()&&searchable.find(needle)==std::string::npos)continue;
                    ImGui::PushID(path.c_str());
                    if(ImGui::Selectable(entry.path().filename().string().c_str(),current==path)){setter(path);ImGui::CloseCurrentPopup();}
                    if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",path.c_str());
                    ImGui::PopID();
                }
            }
            ImGui::EndCombo();
        }
    };

    auto audioPicker=[&](const std::string& current,const std::function<void(const std::string&)>& setter)
    {
        const std::string preview=current.empty()?"None":std::filesystem::path(current).filename().string();
        if(ImGui::BeginCombo("##Value",preview.c_str()))
        {
            static char audioSearch[96]{};
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint("##AudioSearch","Search sounds...",audioSearch,sizeof(audioSearch));
            if(ImGui::Selectable("None",current.empty())){setter("");ImGui::CloseCurrentPopup();}
            std::string needle=audioSearch;
            std::transform(needle.begin(),needle.end(),needle.begin(),[](unsigned char ch){return static_cast<char>(std::tolower(ch));});
            std::error_code ec;const auto assetRoot=findAssetRoot();
            if(std::filesystem::exists(assetRoot,ec))
            {
                for(const auto& entry:std::filesystem::recursive_directory_iterator(assetRoot,ec))
                {
                    if(ec)break;if(!entry.is_regular_file()||entry.path().extension()!=".wav")continue;
                    const auto rel=std::filesystem::path("Assets")/std::filesystem::relative(entry.path(),assetRoot,ec);if(ec)continue;
                    const std::string path=rel.lexically_normal().generic_string();
                    std::string searchable=path;std::transform(searchable.begin(),searchable.end(),searchable.begin(),[](unsigned char ch){return static_cast<char>(std::tolower(ch));});
                    if(!needle.empty()&&searchable.find(needle)==std::string::npos)continue;
                    if(ImGui::Selectable(entry.path().filename().string().c_str(),current==path)){setter(path);ImGui::CloseCurrentPopup();}
                }
            }
            ImGui::EndCombo();
        }
    };

    category("Slot (Canvas)","layout position size anchors alignment pivot z order",true,[&](auto& row)
    {
        row("Position","location",[&]{Vec2 v=widget.GetPosition();if(ImGui::DragFloat2("##Value",&v.x,1.0f))widget.SetPosition(v);});
        row("Size","width height",[&]{Vec2 v=widget.GetSize();if(ImGui::DragFloat2("##Value",&v.x,1.0f,1.0f,10000.0f)){v.x=std::max(1.0f,v.x);v.y=std::max(1.0f,v.y);widget.SetSize(v);}});
        row("Anchors Min","anchor",[&]{Vec2 mn=widget.GetAnchorMinimum(),mx=widget.GetAnchorMaximum();if(ImGui::DragFloat2("##Value",&mn.x,.01f,0.0f,1.0f)){mn.x=std::clamp(mn.x,0.0f,1.0f);mn.y=std::clamp(mn.y,0.0f,1.0f);mx.x=std::max(mx.x,mn.x);mx.y=std::max(mx.y,mn.y);widget.SetAnchors(mn,mx);}});
        row("Anchors Max","anchor",[&]{Vec2 mn=widget.GetAnchorMinimum(),mx=widget.GetAnchorMaximum();if(ImGui::DragFloat2("##Value",&mx.x,.01f,0.0f,1.0f)){mx.x=std::clamp(mx.x,mn.x,1.0f);mx.y=std::clamp(mx.y,mn.y,1.0f);widget.SetAnchors(mn,mx);}});
        row("Anchor Preset","preset stretch fill",[&]
        {
            if(ImGui::Button("Choose...",ImVec2(-1.0f,0)))ImGui::OpenPopup("##AnchorPreset");
            if(ImGui::BeginPopup("##AnchorPreset"))
            {
                struct Preset{const char* label;Vec2 anchor;Vec2 pivot;};
                const Preset presets[]={{"Top Left",{0,0},{0,0}},{"Top",{.5f,0},{.5f,0}},{"Top Right",{1,0},{1,0}},{"Left",{0,.5f},{0,.5f}},{"Center",{.5f,.5f},{.5f,.5f}},{"Right",{1,.5f},{1,.5f}},{"Bottom Left",{0,1},{0,1}},{"Bottom",{.5f,1},{.5f,1}},{"Bottom Right",{1,1},{1,1}}};
                if(ImGui::BeginTable("##AnchorGrid",3,ImGuiTableFlags_SizingStretchSame))
                {
                    for(const auto& preset:presets){ImGui::TableNextColumn();if(ImGui::Button(preset.label,ImVec2(-1,0))){widget.SetAnchor(preset.anchor);widget.SetPivot(preset.pivot);ImGui::CloseCurrentPopup();}}
                    ImGui::EndTable();
                }
                ImGui::Separator();
                Vec2 mn=widget.GetAnchorMinimum(),mx=widget.GetAnchorMaximum();
                if(ImGui::MenuItem("Fill Width"))widget.SetAnchors(Vec2(0,mn.y),Vec2(1,mx.y));
                if(ImGui::MenuItem("Fill Height"))widget.SetAnchors(Vec2(mn.x,0),Vec2(mx.x,1));
                if(ImGui::MenuItem("Fill")){widget.SetAnchors(Vec2(0,0),Vec2(1,1));widget.SetPivot(Vec2(0,0));}
                ImGui::EndPopup();
            }
        });
        row("Alignment / Pivot","pivot alignment",[&]{Vec2 v=widget.GetPivot();if(ImGui::DragFloat2("##Value",&v.x,.01f,0.0f,1.0f)){v.x=std::clamp(v.x,0.0f,1.0f);v.y=std::clamp(v.y,0.0f,1.0f);widget.SetPivot(v);}});
        row("Z Order","layer depth",[&]{int z=widget.GetZOrder();if(ImGui::DragInt("##Value",&z,1.0f,-1000,1000))widget.SetZOrder(z);});
    });

    category("Appearance","color opacity gradient corner radius render",true,[&](auto& row)
    {
        row("Color & Opacity","tint",[&]{Vec4 v=widget.GetColor();if(ImGui::ColorEdit4("##Value",&v.x,ImGuiColorEditFlags_AlphaBar))widget.SetColor(v);});
        row("Render Opacity","opacity alpha",[&]{float v=widget.GetRenderOpacity();if(ImGui::SliderFloat("##Value",&v,0.0f,1.0f,"%.2f"))widget.SetRenderOpacity(v);});
        row("Corner Radius","rounding shape",[&]{float v=widget.GetCornerRadius();if(ImGui::DragFloat("##Value",&v,.5f,0.0f,512.0f,"%.1f px"))widget.SetCornerRadius(v);});
        row("Gradient","color",[&]{bool enabled=widget.HasGradient();if(ImGui::Checkbox("##Value",&enabled))widget.SetGradientEnabled(enabled);});
        if(widget.HasGradient())
        {
            row("Gradient End","color",[&]{Vec4 v=widget.GetGradientColor();if(ImGui::ColorEdit4("##Value",&v.x,ImGuiColorEditFlags_AlphaBar))widget.SetGradientColor(v);});
            row("Gradient Direction","vertical horizontal",[&]{int v=(int)widget.GetGradientDirection();const char* values[]={"Vertical","Horizontal"};if(ImGui::Combo("##Value",&v,values,2))widget.SetGradientDirection((UIGradientDirection)v);});
        }
    });

    if (auto* reusable = dynamic_cast<UIUserWidget*>(&widget))
    {
        category("Reusable Widget", "widget blueprint nested UI asset source", true, [&](auto& row)
        {
            row("Source UI", "path to another .ui asset", [&]
            {
                static std::uint64_t editId = 0;
                static char sourceBuffer[1024]{};
                static std::string lastError;
                if (editId != widget.GetInstanceId())
                {
                    editId = widget.GetInstanceId();
                    std::snprintf(sourceBuffer, sizeof(sourceBuffer), "%s",
                                  reusable->GetSourcePath().c_str());
                    lastError.clear();
                }
                ImGui::SetNextItemWidth(-1.0f);
                const bool entered = ImGui::InputText("##SourceUI", sourceBuffer,
                    sizeof(sourceBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
                if (entered || ImGui::Button("Apply / Reload"))
                {
                    const std::string previous = reusable->GetSourcePath();
                    reusable->SetSourcePath(sourceBuffer);
                    if (!UISerializer::PopulateUserWidget(*reusable, m_UIAssetPath))
                    {
                        reusable->SetSourcePath(previous);
                        lastError = "Missing, invalid or circular .ui asset.";
                    }
                    else lastError.clear();
                }
                if (!lastError.empty())
                    ImGui::TextWrapped("%s", lastError.c_str());
                ImGui::TextDisabled("Example: Assets/UI/Loadout.ui");
                ImGui::TextDisabled("Edit the source UI to update every instance.");
            });
            row("Event Script", "optional controller script for button callbacks", [&]
            {
                static std::uint64_t eventEditId = 0;
                static char eventBuffer[1024]{};
                if (eventEditId != widget.GetInstanceId())
                {
                    eventEditId = widget.GetInstanceId();
                    std::snprintf(eventBuffer, sizeof(eventBuffer), "%s",
                        reusable->GetEventScriptOverride().c_str());
                }
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::InputText("##EventScript", eventBuffer, sizeof(eventBuffer),
                    ImGuiInputTextFlags_EnterReturnsTrue))
                {
                    reusable->SetEventScriptOverride(eventBuffer);
                    UISerializer::PopulateUserWidget(*reusable, m_UIAssetPath);
                }
                ImGui::TextDisabled("Optional: Assets/Scripts/Player.lua");
            });
        });
    }

    if(auto* scroll=dynamic_cast<UIScrollBox*>(&widget))
    {
        category("Scrolling","scroll box viewport content height offset mouse wheel",true,[&](auto& row)
        {
            row("Content Height","vertical scroll extent",[&]{
                float v=scroll->GetContentHeight();
                if(ImGui::DragFloat("##Value",&v,2.0f,0.0f,100000.0f))
                    scroll->SetContentHeight(v);
            });
            row("Scroll Offset","vertical position",[&]{
                float v=scroll->GetScrollOffset();
                if(ImGui::DragFloat("##Value",&v,2.0f,0.0f,100000.0f))
                    scroll->SetScrollOffset(v);
            });
        });
    }

    if(auto* text=dynamic_cast<UIText*>(&widget))
    {
        category("Content","text string value",true,[&](auto& row)
        {
            row("Text","content value",[&]{char buffer[1024]{};std::snprintf(buffer,sizeof(buffer),"%s",text->GetText().c_str());if(ImGui::InputTextMultiline("##Value",buffer,sizeof(buffer),ImVec2(-1,68)))text->SetText(buffer);});
        });
        category("Font","font size alignment typography",true,[&](auto& row)
        {
            row("Font Size","size",[&]{float v=text->GetFontSize();if(ImGui::DragFloat("##Value",&v,.5f,1.0f,512.0f,"%.0f"))text->SetFontSize(v);});
            row("Horizontal Alignment","left center right",[&]{int v=(int)text->GetHorizontalAlignment();const char* values[]={"Left","Center","Right"};if(ImGui::Combo("##Value",&v,values,3))text->SetHorizontalAlignment((UITextHorizontalAlignment)v);});
            row("Vertical Alignment","top center bottom",[&]{int v=(int)text->GetVerticalAlignment();const char* values[]={"Top","Center","Bottom"};if(ImGui::Combo("##Value",&v,values,3))text->SetVerticalAlignment((UITextVerticalAlignment)v);});
        });
    }

    if(auto* input=dynamic_cast<UITextInput*>(&widget))
    {
        category("Content","text input placeholder password max length font",true,[&](auto& row)
        {
            row("Text","value",[&]{char buffer[512]{};std::snprintf(buffer,sizeof(buffer),"%s",input->GetText().c_str());if(ImGui::InputText("##Value",buffer,sizeof(buffer)))input->SetText(buffer);});
            row("Placeholder","hint",[&]{char buffer[512]{};std::snprintf(buffer,sizeof(buffer),"%s",input->GetPlaceholder().c_str());if(ImGui::InputText("##Value",buffer,sizeof(buffer)))input->SetPlaceholder(buffer);});
            row("Font Size","font",[&]{float v=input->GetFontSize();if(ImGui::DragFloat("##Value",&v,.5f,1.0f,256.0f,"%.0f"))input->SetFontSize(v);});
            row("Max Length","characters",[&]{int v=(int)input->GetMaxLength();if(ImGui::DragInt("##Value",&v,1,1,16384))input->SetMaxLength((std::size_t)std::max(v,1));});
            row("Password","mask secret",[&]{bool v=input->IsPassword();if(ImGui::Checkbox("##Value",&v))input->SetPassword(v);});
        });
    }

    if(auto* image=dynamic_cast<UIImage*>(&widget))
    {
        category("Brush","image texture asset",true,[&](auto& row)
        {
            row("Image","texture asset",[&]{texturePicker(image->GetTexturePath(),[&](const std::string& v){image->SetTexturePath(v);});});
            if(m_DetailsShowAdvanced && !image->GetTexturePath().empty() && m_Renderer)
                row("Preview","thumbnail",[&]{Texture2D* tex=m_Renderer->LoadTexture(image->GetTexturePath());if(tex&&tex->IsLoaded())ImGui::Image((ImTextureID)(intptr_t)tex->GetID(),ImVec2(52,52),ImVec2(0,1),ImVec2(1,0));});
        });
    }

    if(auto* slider=dynamic_cast<UISlider*>(&widget))
    {
        category("Slider","value fill handle style",true,[&](auto& row)
        {
            row("Value","percent",[&]{float v=slider->GetValue();if(ImGui::SliderFloat("##Value",&v,0.0f,1.0f,"%.2f"))slider->SetValue(v);});
            row("Fill Color","color",[&]{Vec4 v=slider->GetFillColor();if(ImGui::ColorEdit4("##Value",&v.x))slider->SetFillColor(v);});
            row("Handle Color","color",[&]{Vec4 v=slider->GetHandleColor();if(ImGui::ColorEdit4("##Value",&v.x))slider->SetHandleColor(v);});
        });
    }

    if(auto* progress=dynamic_cast<UIProgressBar*>(&widget))
    {
        category("Progress","percent fill direction style",true,[&](auto& row)
        {
            row("Percent","value progress",[&]{float v=progress->GetPercent();if(ImGui::SliderFloat("##Value",&v,0.0f,1.0f,"%.2f"))progress->SetPercent(v);});
            row("Fill Color","color",[&]{Vec4 v=progress->GetFillColor();if(ImGui::ColorEdit4("##Value",&v.x))progress->SetFillColor(v);});
            row("Fill Direction","left right top bottom",[&]{int v=(int)progress->GetFillDirection();const char* values[]={"Left to Right","Right to Left","Top to Bottom","Bottom to Top"};if(ImGui::Combo("##Value",&v,values,4))progress->SetFillDirection((UIProgressBarFillDirection)v);});
        });
    }

    if(auto* button=dynamic_cast<UIButton*>(&widget))
    {
        category("Style","button normal hovered pressed disabled color",true,[&](auto& row)
        {
            row("Normal","color",[&]{Vec4 v=button->GetNormalColor();if(ImGui::ColorEdit4("##Value",&v.x))button->SetNormalColor(v);});
            row("Hovered","color",[&]{Vec4 v=button->GetHoveredColor();if(ImGui::ColorEdit4("##Value",&v.x))button->SetHoveredColor(v);});
            row("Pressed","color",[&]{Vec4 v=button->GetPressedColor();if(ImGui::ColorEdit4("##Value",&v.x))button->SetPressedColor(v);});
            row("Disabled","color",[&]{Vec4 v=button->GetDisabledColor();if(ImGui::ColorEdit4("##Value",&v.x))button->SetDisabledColor(v);});
        });
        category("Brushes","button images normal hovered pressed disabled",false,[&](auto& row)
        {
            row("Normal Image","texture",[&]{texturePicker(button->GetNormalImage(),[&](const std::string& v){button->SetNormalImage(v);});});
            row("Hovered Image","texture",[&]{texturePicker(button->GetHoveredImage(),[&](const std::string& v){button->SetHoveredImage(v);});});
            row("Pressed Image","texture",[&]{texturePicker(button->GetPressedImage(),[&](const std::string& v){button->SetPressedImage(v);});});
            row("Disabled Image","texture",[&]{texturePicker(button->GetDisabledImage(),[&](const std::string& v){button->SetDisabledImage(v);});});
        });
        category("Interaction","click sound child text highlight",false,[&](auto& row)
        {
            row("Click Sound","audio wav",[&]{audioPicker(button->GetClickSoundPath(),[&](const std::string& v){button->SetClickSoundPath(v);});});
            row("Affect Child Text","highlight text",[&]{bool v=button->GetAffectChildText();if(ImGui::Checkbox("##Value",&v))button->SetAffectChildText(v);});
            if(button->GetAffectChildText())
            {
                row("Text Normal","color",[&]{Vec4 v=button->GetNormalTextColor();if(ImGui::ColorEdit4("##Value",&v.x))button->SetNormalTextColor(v);});
                row("Text Hovered","color",[&]{Vec4 v=button->GetHoveredTextColor();if(ImGui::ColorEdit4("##Value",&v.x))button->SetHoveredTextColor(v);});
                row("Text Pressed","color",[&]{Vec4 v=button->GetPressedTextColor();if(ImGui::ColorEdit4("##Value",&v.x))button->SetPressedTextColor(v);});
                row("Text Disabled","color",[&]{Vec4 v=button->GetDisabledTextColor();if(ImGui::ColorEdit4("##Value",&v.x))button->SetDisabledTextColor(v);});
            }
        });
        category("Events","on click script function event",false,[&](auto& row)
        {
            row("On Click Script","lua asset",[&]{char buffer[256]{};std::snprintf(buffer,sizeof(buffer),"%s",button->GetOnClickScript().c_str());if(ImGui::InputTextWithHint("##Value","Assets/Scripts/MainMenu.lua",buffer,sizeof(buffer)))button->SetOnClickScript(buffer);});
            row("On Click Function","lua callback",[&]{char buffer[128]{};std::snprintf(buffer,sizeof(buffer),"%s",button->GetOnClickFunction().c_str());if(ImGui::InputTextWithHint("##Value","OnClicked",buffer,sizeof(buffer)))button->SetOnClickFunction(buffer);});
        });
        if(m_DetailsShowAdvanced)
        {
            category("Runtime State","debug hovered pressed",false,[&](auto& row)
            {
                row("Hovered","debug",[&]{bool v=button->IsHovered();ImGui::BeginDisabled();ImGui::Checkbox("##Value",&v);ImGui::EndDisabled();});
                row("Pressed","debug",[&]{bool v=button->IsPressed();ImGui::BeginDisabled();ImGui::Checkbox("##Value",&v);ImGui::EndDisabled();});
            });
        }
    }

    category("Behavior","visibility enabled hit test interaction",false,[&](auto& row)
    {
        row("Visibility","visible hidden hit test invisible",[&]
        {
            int visibility=!widget.IsVisible()?2:(widget.IsHitTestVisible()?0:1);
            const char* values[]={"Visible","Hit Test Invisible","Hidden"};
            if(ImGui::Combo("##Value",&visibility,values,3))
            {
                widget.SetVisible(visibility!=2);
                widget.SetHitTestVisible(visibility==0);
            }
        });
        row("Enabled","interaction disabled",[&]{bool v=widget.IsEnabled();if(ImGui::Checkbox("##Value",&v))widget.SetEnabled(v);});
        if(m_DetailsShowAdvanced)
            row("Hit Test Visible","input mouse",[&]{bool v=widget.IsHitTestVisible();if(ImGui::Checkbox("##Value",&v))widget.SetHitTestVisible(v);});
    });

    m_DetailsExpandRequest=0;
    ImGui::Dummy(ImVec2(0.0f,6.0f));
    ImGui::PopStyleVar(4);
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

    ImGui::SameLine();
    if(ImGui::Button("Screen")) ImGui::OpenPopup("UIScreenPresetPopup");
    if(ImGui::BeginPopup("UIScreenPresetPopup"))
    {
        ImGui::TextDisabled("Preview Size");
        struct ScreenPreset { const char* name; float w; float h; };
        const ScreenPreset presets[]={{"720p (1280 x 720)",1280,720},{"1080p (1920 x 1080)",1920,1080},{"1440p (2560 x 1440)",2560,1440},{"4K (3840 x 2160)",3840,2160},{"Portrait (1080 x 1920)",1080,1920}};
        for(const auto& preset:presets) if(ImGui::MenuItem(preset.name)) canvas.SetSize(Vec2(preset.w,preset.h));
        ImGui::Separator();
        Vec2 custom=canvas.GetSize();
        ImGui::SetNextItemWidth(180.0f);
        if(ImGui::DragFloat2("Custom",&custom.x,1.0f,64.0f,8192.0f,"%.0f")) canvas.SetSize(Vec2(std::max(64.0f,custom.x),std::max(64.0f,custom.y)));
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
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5.0f, 3.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 4.0f));
    ImGui::BeginChild("##DesignerViewportToolbar", ImVec2(0.0f, 30.0f), false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImGui::TextDisabled("%.0f x %.0f", canvas.GetSize().x, canvas.GetSize().y);
    if (m_SelectedWidget)
    {
        ImGui::SameLine();
        ImGui::TextDisabled(" | ");
        ImGui::SameLine(0.0f, 2.0f);
        ImGui::TextUnformatted(m_SelectedWidget->GetName().c_str());
    }

    const float rightControls = 238.0f;
    if (ImGui::GetContentRegionAvail().x > rightControls)
        ImGui::SameLine(ImGui::GetWindowWidth() - rightControls);

    if (ImGui::SmallButton(m_ShowGrid ? "Grid" : "Grid Off"))
        m_ShowGrid = !m_ShowGrid;
    ImGui::SameLine();
    if (ImGui::SmallButton(m_SnapToGrid ? "Snap" : "Snap Off"))
        m_SnapToGrid = !m_SnapToGrid;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.0f);
    ImGui::SliderFloat("##DesignerZoom", &m_Zoom, 0.20f, 5.0f, "%.0fx");

    ImGui::EndChild();
    ImGui::PopStyleVar(2);

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
    const float fitScale = std::min(scaleX, scaleY);

    // Keep the canvas readable when this dock is small. The Designer becomes
    // a viewport onto the authored UI instead of shrinking it to a thumbnail.
    const float readableWidth = 720.0f;
    const float readableHeight = 405.0f;
    const float readableScale = std::min(
        canvasSize.x > 0.0f ? readableWidth / canvasSize.x : 1.0f,
        canvasSize.y > 0.0f ? readableHeight / canvasSize.y : 1.0f);

    float scale = std::max(fitScale, std::min(1.0f, readableScale));
    scale *= m_Zoom;
    scale = std::max(0.05f, scale);

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
     * Canvas background. Layered outer strokes give the design surface a
     * little depth without stealing useful viewport space.
     */
    const ImVec2 canvasMax(
        canvasPosition.x + canvasPixelSize.x,
        canvasPosition.y + canvasPixelSize.y);
    drawList->AddRectFilled(
        ImVec2(canvasPosition.x - 10.0f, canvasPosition.y - 10.0f),
        ImVec2(canvasMax.x + 10.0f, canvasMax.y + 10.0f),
        IM_COL32(0, 0, 0, 22), 3.0f);
    drawList->AddRectFilled(
        ImVec2(canvasPosition.x - 5.0f, canvasPosition.y - 5.0f),
        ImVec2(canvasMax.x + 5.0f, canvasMax.y + 5.0f),
        IM_COL32(0, 0, 0, 35), 2.0f);
    drawList->AddRectFilled(
        canvasPosition,
        canvasMax,
        IM_COL32(37, 40, 47, 255)
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
                    (static_cast<int>(std::round((x - canvasPosition.x) / gridSpacing)) % 5 == 0)
                        ? IM_COL32(255,255,255,34)
                        : IM_COL32(255,255,255,16)
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
                    (static_cast<int>(std::round((y - canvasPosition.y) / gridSpacing)) % 5 == 0)
                        ? IM_COL32(255,255,255,34)
                        : IM_COL32(255,255,255,16)
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
        if (m_SelectedWidget && !m_Dragging && !m_Resizing)
        {
            const UIRect selectedRect = GetAbsoluteRect(
                *m_SelectedWidget,
                UIRect{0.0f,0.0f,canvasSize.x,canvasSize.y});
            const int hoveredHandle = GetResizeHandle(selectedRect, mouse, canvasPosition, scale);
            if (hoveredHandle == 0 || hoveredHandle == 7) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);
            else if (hoveredHandle == 2 || hoveredHandle == 5) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNESW);
            else if (hoveredHandle == 1 || hoveredHandle == 6) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
            else if (hoveredHandle == 3 || hoveredHandle == 4) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }

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
        if(ImGui::MenuItem("Scroll Box")) { PushHistory(canvas); AddWidget(canvas,UIWidgetType::ScrollBox); }
        if(ImGui::MenuItem("User Widget")) { PushHistory(canvas); AddWidget(canvas,UIWidgetType::UserWidget); }
        if(ImGui::MenuItem("Text")) { PushHistory(canvas); AddWidget(canvas,UIWidgetType::Text); }
        if(ImGui::MenuItem("Image")) { PushHistory(canvas); AddWidget(canvas,UIWidgetType::Image); }
        if(ImGui::MenuItem("Button")) { PushHistory(canvas); AddWidget(canvas,UIWidgetType::Button); }
        if(ImGui::MenuItem("Text Input")) { PushHistory(canvas); AddWidget(canvas,UIWidgetType::TextInput); }
        if(ImGui::MenuItem("Slider")) { PushHistory(canvas); AddWidget(canvas,UIWidgetType::Slider); }
        if(ImGui::MenuItem("Progress Bar")) { PushHistory(canvas); AddWidget(canvas,UIWidgetType::ProgressBar); }
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
                    value <= static_cast<int>(UIWidgetType::ScrollBox))
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
    float inheritedOpacity=1.0f;
    for(const UIWidget* current=&widget;current;current=current->GetParent()) inheritedOpacity*=current->GetRenderOpacity();
    color.w*=std::clamp(inheritedOpacity,0.0f,1.0f);

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

            std::vector<std::string> lines;
            std::string line;
            const std::string& previewText = text->GetText();
            for (std::size_t i = 0; i <= previewText.size(); ++i)
            {
                if (i == previewText.size() || previewText[i] == '\n')
                {
                    lines.push_back(line);
                    line.clear();
                }
                else if (previewText[i] != '\r') line += previewText[i];
            }
            const float lineHeight = previewSize * 1.2f;
            const float blockHeight = previewSize + (lines.size() > 1 ? (lines.size()-1)*lineHeight : 0.0f);
            float lineY=min.y;
            if(text->GetVerticalAlignment()==UITextVerticalAlignment::Center) lineY += ((max.y-min.y)-blockHeight)*0.5f;
            else if(text->GetVerticalAlignment()==UITextVerticalAlignment::Bottom) lineY += (max.y-min.y)-blockHeight;
            for(const std::string& value:lines)
            {
                const ImVec2 measured=previewFont->CalcTextSizeA(previewSize,FLT_MAX,0.0f,value.c_str());
                float lineX=min.x;
                if(text->GetHorizontalAlignment()==UITextHorizontalAlignment::Center) lineX += ((max.x-min.x)-measured.x)*0.5f;
                else if(text->GetHorizontalAlignment()==UITextHorizontalAlignment::Right) lineX += (max.x-min.x)-measured.x;
                drawList->AddText(previewFont,previewSize,ImVec2(lineX,lineY),fillColor,value.c_str());
                lineY += lineHeight;
            }
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
            const float previewRadius=std::max(0.0f,widget.GetCornerRadius()*scale);
            if(previewRadius>0.0f)
                drawList->AddImageRounded((ImTextureID)(intptr_t)texture->GetID(),min,max,
                    ImVec2(0.0f,1.0f),ImVec2(1.0f,0.0f),fillColor,previewRadius);
            else
                drawList->AddImage((ImTextureID)(intptr_t)texture->GetID(),min,max,
                    ImVec2(0.0f,1.0f),ImVec2(1.0f,0.0f),fillColor);
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
        if (widget.HasGradient())
        {
            const Vec4 gc = widget.GetGradientColor();
            const ImU32 endColor = IM_COL32((int)(std::clamp(gc.x,0.0f,1.0f)*255.0f),(int)(std::clamp(gc.y,0.0f,1.0f)*255.0f),(int)(std::clamp(gc.z,0.0f,1.0f)*255.0f),(int)(std::clamp(gc.w*inheritedOpacity,0.0f,1.0f)*255.0f));
            if (widget.GetGradientDirection() == UIGradientDirection::Horizontal)
                drawList->AddRectFilledMultiColor(min,max,fillColor,endColor,endColor,fillColor);
            else
                drawList->AddRectFilledMultiColor(min,max,fillColor,fillColor,endColor,endColor);
        }
        else
        {
            const float previewRadius=std::max(0.0f,widget.GetCornerRadius()*scale);
            drawList->AddRectFilled(min,max,fillColor,previewRadius);
        }

        const float previewRadius=std::max(0.0f,widget.GetCornerRadius()*scale);
        drawList->AddRect(min,max,IM_COL32(255,255,255,70),previewRadius);

        if(auto* progress=dynamic_cast<UIProgressBar*>(&widget))
        {
            const float value=std::clamp(progress->GetPercent(),0.0f,1.0f);
            ImVec2 fillMin=min,fillMax=max;
            switch(progress->GetFillDirection())
            {
            case UIProgressBarFillDirection::LeftToRight: fillMax.x=min.x+(max.x-min.x)*value; break;
            case UIProgressBarFillDirection::RightToLeft: fillMin.x=max.x-(max.x-min.x)*value; break;
            case UIProgressBarFillDirection::TopToBottom: fillMax.y=min.y+(max.y-min.y)*value; break;
            case UIProgressBarFillDirection::BottomToTop: fillMin.y=max.y-(max.y-min.y)*value; break;
            }
            Vec4 fc=progress->GetFillColor();
            const ImU32 fill=IM_COL32((int)(std::clamp(fc.x,0.0f,1.0f)*255),(int)(std::clamp(fc.y,0.0f,1.0f)*255),(int)(std::clamp(fc.z,0.0f,1.0f)*255),(int)(std::clamp(fc.w*inheritedOpacity,0.0f,1.0f)*255));
            drawList->AddRectFilled(fillMin,fillMax,fill,previewRadius);
        }

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

        constexpr float handleSize = 4.0f;
        const float midX=(min.x+max.x)*0.5f;
        const float midY=(min.y+max.y)*0.5f;
        const ImVec2 handles[] =
        {
            ImVec2(min.x,min.y),
            ImVec2(midX,min.y),
            ImVec2(max.x,min.y),
            ImVec2(min.x,midY),
            ImVec2(max.x,midY),
            ImVec2(min.x,max.y),
            ImVec2(midX,max.y),
            ImVec2(max.x,max.y)
        };

        for (const ImVec2& handle : handles)
        {
            drawList->AddRectFilled(
                ImVec2(handle.x-handleSize,handle.y-handleSize),
                ImVec2(handle.x+handleSize,handle.y+handleSize),
                IM_COL32(230, 241, 255, 255));
            drawList->AddRect(
                ImVec2(handle.x-handleSize,handle.y-handleSize),
                ImVec2(handle.x+handleSize,handle.y+handleSize),
                IM_COL32(45, 119, 205, 255));
        }

        char selectionLabel[320]{};
        std::snprintf(selectionLabel,sizeof(selectionLabel),"%s   %.0f x %.0f",
            widget.GetName().c_str(),rect.width,rect.height);
        const ImVec2 labelSize=ImGui::CalcTextSize(selectionLabel);
        const float labelY=std::max(canvasPosition.y,min.y-labelSize.y-9.0f);
        const ImVec2 labelMin(min.x,labelY);
        const ImVec2 labelMax(min.x+labelSize.x+12.0f,labelY+labelSize.y+6.0f);
        drawList->AddRectFilled(labelMin,labelMax,IM_COL32(31,88,150,235),3.0f);
        drawList->AddText(ImVec2(labelMin.x+6.0f,labelMin.y+3.0f),IM_COL32(240,247,255,255),selectionLabel);
    }

    UIRect childRect = rect;
    const auto* scroll = dynamic_cast<const UIScrollBox*>(&widget);
    if (scroll)
    {
        childRect.y -= scroll->GetScrollOffset();
        drawList->PushClipRect(min,max,true);
    }
    for (const auto& child : widget.GetChildren())
        if (child) DrawWidget(*child,childRect,canvasPosition,scale,drawList);
    if (scroll) drawList->PopClipRect();
}

void UIEditor::SelectWidget(
    UIWidget* widget)
{
    // Linked content is edited in its source blueprint, never as an
    // unsaved override in a host screen.
    for (UIWidget* parent = widget ? widget->GetParent() : nullptr;
         parent; parent = parent->GetParent())
        if (parent->GetType() == UIWidgetType::UserWidget)
        {
            widget = parent;
            break;
        }
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
    case 0: position.x+=deltaX; position.y+=deltaY; size.x-=deltaX; size.y-=deltaY; break; // top-left
    case 1: position.y+=deltaY; size.y-=deltaY; break;                                     // top
    case 2: position.y+=deltaY; size.x+=deltaX; size.y-=deltaY; break;                     // top-right
    case 3: position.x+=deltaX; size.x-=deltaX; break;                                     // left
    case 4: size.x+=deltaX; break;                                                          // right
    case 5: position.x+=deltaX; size.x-=deltaX; size.y+=deltaY; break;                     // bottom-left
    case 6: size.y+=deltaY; break;                                                          // bottom
    case 7: size.x+=deltaX; size.y+=deltaY; break;                                          // bottom-right
    }

    constexpr float minimumSize = 1.0f;
    if (size.x < minimumSize)
    {
        if (m_ResizeHandle==0 || m_ResizeHandle==3 || m_ResizeHandle==5)
            position.x=m_DragStartPosition.x+m_DragStartSize.x-minimumSize;
        size.x=minimumSize;
    }
    if (size.y < minimumSize)
    {
        if (m_ResizeHandle==0 || m_ResizeHandle==1 || m_ResizeHandle==2)
            position.y=m_DragStartPosition.y+m_DragStartSize.y-minimumSize;
        size.y=minimumSize;
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

    constexpr float handleRadius = 9.0f;
    const float midX=(min.x+max.x)*0.5f;
    const float midY=(min.y+max.y)*0.5f;
    const ImVec2 handles[] =
    {
        ImVec2(min.x,min.y),
        ImVec2(midX,min.y),
        ImVec2(max.x,min.y),
        ImVec2(min.x,midY),
        ImVec2(max.x,midY),
        ImVec2(min.x,max.y),
        ImVec2(midX,max.y),
        ImVec2(max.x,max.y)
    };

    for (int i = 0; i < 8; ++i)
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
        UIWidgetType::Button || parent->GetType() == UIWidgetType::UserWidget || parent->GetType() == UIWidgetType::TextInput || parent->GetType() == UIWidgetType::Slider || parent->GetType() == UIWidgetType::ProgressBar)
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

    Vec2 defaultSize(200.0f,100.0f);
    switch(type)
    {
    case UIWidgetType::Panel: defaultSize=Vec2(320.0f,220.0f); break;
    case UIWidgetType::ScrollBox: defaultSize=Vec2(420.0f,300.0f); break;
    case UIWidgetType::UserWidget: defaultSize=Vec2(1920.0f,1080.0f); break;
    case UIWidgetType::Text: defaultSize=Vec2(240.0f,48.0f); break;
    case UIWidgetType::Image: defaultSize=Vec2(180.0f,180.0f); break;
    case UIWidgetType::Button: defaultSize=Vec2(220.0f,48.0f); break;
    case UIWidgetType::TextInput: defaultSize=Vec2(280.0f,38.0f); break;
    case UIWidgetType::Slider: defaultSize=Vec2(280.0f,24.0f); break;
    case UIWidgetType::ProgressBar: defaultSize=Vec2(280.0f,24.0f); break;
    }
    widget->SetSize(defaultSize);

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
        return;

    std::snprintf(m_RenameBuffer, sizeof(m_RenameBuffer), "%s", m_SelectedWidget->GetName().c_str());
    m_RenameRequested = true;
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

    UIRect parentRect = GetAbsoluteRect(*parent, canvasRect);
    if (const auto* scroll = dynamic_cast<const UIScrollBox*>(parent))
        parentRect.y -= scroll->GetScrollOffset();
    return UILayout::Calculate(widget, parentRect);
}

std::unique_ptr<UIWidget> UIEditor::CloneWidget(const UIWidget& source) const
{
    std::unique_ptr<UIWidget> copy=UIWidgetFactory::Create(source.GetType());
    if(!copy) return nullptr;
    copy->SetName(source.GetName()); copy->SetPosition(source.GetPosition()); copy->SetSize(source.GetSize());
    copy->SetAnchors(source.GetAnchorMinimum(),source.GetAnchorMaximum()); copy->SetPivot(source.GetPivot());
    copy->SetColor(source.GetColor()); copy->SetVisible(source.IsVisible()); copy->SetEnabled(source.IsEnabled());
    copy->SetHitTestVisible(source.IsHitTestVisible()); copy->SetZOrder(source.GetZOrder()); copy->SetRenderOpacity(source.GetRenderOpacity());
    copy->SetCornerRadius(source.GetCornerRadius()); copy->SetGradientEnabled(source.HasGradient());
    copy->SetGradientColor(source.GetGradientColor()); copy->SetGradientDirection(source.GetGradientDirection());
    if(auto* a=dynamic_cast<const UIUserWidget*>(&source)) if(auto* b=dynamic_cast<UIUserWidget*>(copy.get())) { b->SetSourcePath(a->GetSourcePath()); b->SetEventScriptOverride(a->GetEventScriptOverride()); }
    if(auto* a=dynamic_cast<const UIScrollBox*>(&source)) if(auto* b=dynamic_cast<UIScrollBox*>(copy.get())) { b->SetContentHeight(a->GetContentHeight()); b->SetScrollOffset(a->GetScrollOffset()); }
    if(auto* a=dynamic_cast<const UIText*>(&source)) if(auto* b=dynamic_cast<UIText*>(copy.get())) { b->SetText(a->GetText()); b->SetFontSize(a->GetFontSize()); b->SetHorizontalAlignment(a->GetHorizontalAlignment()); b->SetVerticalAlignment(a->GetVerticalAlignment()); }
    if(auto* a=dynamic_cast<const UITextInput*>(&source)) if(auto* b=dynamic_cast<UITextInput*>(copy.get())) { b->SetText(a->GetText()); b->SetPlaceholder(a->GetPlaceholder()); b->SetFontSize(a->GetFontSize()); b->SetMaxLength(a->GetMaxLength()); b->SetPassword(a->IsPassword()); }
    if(auto* a=dynamic_cast<const UISlider*>(&source)) if(auto* b=dynamic_cast<UISlider*>(copy.get())) { b->SetValue(a->GetValue()); b->SetFillColor(a->GetFillColor()); b->SetHandleColor(a->GetHandleColor()); }
    if(auto* a=dynamic_cast<const UIProgressBar*>(&source)) if(auto* b=dynamic_cast<UIProgressBar*>(copy.get())) { b->SetPercent(a->GetPercent()); b->SetFillColor(a->GetFillColor()); b->SetFillDirection(a->GetFillDirection()); }
    if(auto* a=dynamic_cast<const UIImage*>(&source)) if(auto* b=dynamic_cast<UIImage*>(copy.get())) b->SetTexturePath(a->GetTexturePath());
    if(auto* a=dynamic_cast<const UIButton*>(&source)) if(auto* b=dynamic_cast<UIButton*>(copy.get())) {
        b->SetNormalColor(a->GetNormalColor()); b->SetHoveredColor(a->GetHoveredColor());
        b->SetPressedColor(a->GetPressedColor()); b->SetDisabledColor(a->GetDisabledColor());
        b->SetAffectChildText(a->GetAffectChildText()); b->SetNormalTextColor(a->GetNormalTextColor());
        b->SetHoveredTextColor(a->GetHoveredTextColor()); b->SetPressedTextColor(a->GetPressedTextColor()); b->SetDisabledTextColor(a->GetDisabledTextColor());
        b->SetClickSoundPath(a->GetClickSoundPath()); b->SetOnClickScript(a->GetOnClickScript()); b->SetOnClickFunction(a->GetOnClickFunction());
        b->SetNormalImage(a->GetNormalImage()); b->SetHoveredImage(a->GetHoveredImage()); b->SetPressedImage(a->GetPressedImage()); b->SetDisabledImage(a->GetDisabledImage());
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
    const std::filesystem::path temp=std::filesystem::temp_directory_path()/"engine_ui_editor_history.ui";
    if(!UISerializer::Save(canvas,temp.string())) return {};
    std::ifstream in(temp,std::ios::binary); return std::string((std::istreambuf_iterator<char>(in)),{});
}

bool UIEditor::RestoreCanvas(UICanvas& canvas,const std::string& snapshot)
{
    if(snapshot.empty()) return false;
    const std::filesystem::path temp=std::filesystem::temp_directory_path()/"engine_ui_editor_history.ui";
    { std::ofstream out(temp,std::ios::binary|std::ios::trunc); out<<snapshot; }
    m_SelectedWidget=nullptr; m_Dragging=false; m_Resizing=false;
    return UISerializer::Load(canvas,temp.string(),m_UIAssetPath);
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
