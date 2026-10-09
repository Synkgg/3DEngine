#include "UISerializer.h"

#include "UIButton.h"
#include "UIImage.h"
#include "UIText.h"
#include "UITextInput.h"
#include "UISlider.h"
#include "UIProgressBar.h"
#include "UIScrollBox.h"
#include "UIWidgetFactory.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <vector>

namespace
{
    std::string EncodeText(std::string value)
    {
        std::string result;
        result.reserve(value.size());
        for (char ch : value)
        {
            if (ch == '\n') result += "\\n";
            else if (ch == '\r') result += "\\r";
            else result += ch;
        }
        return result;
    }

    std::string DecodeText(const std::string& value)
    {
        std::string result;
        result.reserve(value.size());
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            if (value[i] == '\\' && i + 1 < value.size())
            {
                if (value[i + 1] == 'n') { result += '\n'; ++i; continue; }
                if (value[i + 1] == 'r') { result += '\r'; ++i; continue; }
            }
            result += value[i];
        }
        return result;
    }

    void WriteWidget(std::ostream& out, const UIWidget& widget, int depth)
    {
        const Vec2 pos = widget.GetPosition();
        const Vec2 size = widget.GetSize();
        const Vec2 amin = widget.GetAnchorMinimum();
        const Vec2 amax = widget.GetAnchorMaximum();
        const Vec2 pivot = widget.GetPivot();
        const Vec4 color = widget.GetColor();

        out << depth << ' ' << static_cast<int>(widget.GetType()) << ' '
            << std::quoted(widget.GetName()) << ' '
            << pos.x << ' ' << pos.y << ' ' << size.x << ' ' << size.y << ' '
            << amin.x << ' ' << amin.y << ' ' << amax.x << ' ' << amax.y << ' '
            << pivot.x << ' ' << pivot.y << ' '
            << color.x << ' ' << color.y << ' ' << color.z << ' ' << color.w << ' '
            << widget.IsVisible() << ' ' << widget.IsEnabled() << ' '
            << widget.IsHitTestVisible() << ' ' << widget.GetZOrder()
            << ' ' << widget.HasGradient() << ' ' << widget.GetGradientColor().x << ' ' << widget.GetGradientColor().y << ' ' << widget.GetGradientColor().z << ' ' << widget.GetGradientColor().w << ' ' << static_cast<int>(widget.GetGradientDirection()) << ' ' << widget.GetCornerRadius()
            << ' ' << widget.GetRenderOpacity();

        if (const UIScrollBox* scroll = dynamic_cast<const UIScrollBox*>(&widget))
            out << ' ' << scroll->GetContentHeight() << ' ' << scroll->GetScrollOffset();
        else if (const UIText* text = dynamic_cast<const UIText*>(&widget))
            out << ' ' << std::quoted(EncodeText(text->GetText())) << ' ' << text->GetFontSize() << ' ' << static_cast<int>(text->GetHorizontalAlignment()) << ' ' << static_cast<int>(text->GetVerticalAlignment());
        else if (const UITextInput* input = dynamic_cast<const UITextInput*>(&widget))
            out << ' ' << std::quoted(input->GetText()) << ' ' << std::quoted(input->GetPlaceholder())
                << ' ' << input->GetFontSize() << ' ' << input->GetMaxLength() << ' ' << input->IsPassword();
        else if (const UISlider* slider = dynamic_cast<const UISlider*>(&widget))
        {
            const Vec4 fill=slider->GetFillColor(), handle=slider->GetHandleColor();
            out << ' ' << slider->GetValue()
                << ' ' << fill.x << ' ' << fill.y << ' ' << fill.z << ' ' << fill.w
                << ' ' << handle.x << ' ' << handle.y << ' ' << handle.z << ' ' << handle.w;
        }
        else if (const UIProgressBar* progress = dynamic_cast<const UIProgressBar*>(&widget))
        {
            const Vec4 fill=progress->GetFillColor();
            out << ' ' << progress->GetPercent()
                << ' ' << fill.x << ' ' << fill.y << ' ' << fill.z << ' ' << fill.w
                << ' ' << static_cast<int>(progress->GetFillDirection());
        }
        else if (const UIImage* image = dynamic_cast<const UIImage*>(&widget))
            out << ' ' << std::quoted(image->GetTexturePath());
        else if (const UIButton* button = dynamic_cast<const UIButton*>(&widget))
        {
            const Vec4 normal = button->GetNormalColor();
            const Vec4 hovered = button->GetHoveredColor();
            const Vec4 pressed = button->GetPressedColor();
            const Vec4 disabled = button->GetDisabledColor();
            out << ' ' << normal.x << ' ' << normal.y << ' ' << normal.z << ' ' << normal.w
                << ' ' << hovered.x << ' ' << hovered.y << ' ' << hovered.z << ' ' << hovered.w
                << ' ' << pressed.x << ' ' << pressed.y << ' ' << pressed.z << ' ' << pressed.w
                << ' ' << disabled.x << ' ' << disabled.y << ' ' << disabled.z << ' ' << disabled.w;
            const Vec4 tn=button->GetNormalTextColor(), th=button->GetHoveredTextColor(), tp=button->GetPressedTextColor(), td=button->GetDisabledTextColor();
            out << ' ' << button->GetAffectChildText()
                << ' ' << tn.x << ' ' << tn.y << ' ' << tn.z << ' ' << tn.w
                << ' ' << th.x << ' ' << th.y << ' ' << th.z << ' ' << th.w
                << ' ' << tp.x << ' ' << tp.y << ' ' << tp.z << ' ' << tp.w
                << ' ' << td.x << ' ' << td.y << ' ' << td.z << ' ' << td.w
                << ' ' << std::quoted(button->GetClickSoundPath())
                << ' ' << std::quoted(button->GetOnClickScript())
                << ' ' << std::quoted(button->GetOnClickFunction())
                << ' ' << std::quoted(button->GetNormalImage()) << ' ' << std::quoted(button->GetHoveredImage())
                << ' ' << std::quoted(button->GetPressedImage()) << ' ' << std::quoted(button->GetDisabledImage());
        }

        out << '\n';

        for (const auto& child : widget.GetChildren())
            if (child) WriteWidget(out, *child, depth + 1);
    }
}

bool UISerializer::Save(const UICanvas& canvas, const std::string& filepath)
{
    const std::filesystem::path outputPath(filepath);
    if (outputPath.has_parent_path())
    {
        std::error_code error;
        std::filesystem::create_directories(outputPath.parent_path(), error);
        if (error) return false;
    }

    std::ofstream out(filepath);
    if (!out) return false;

    const Vec2 canvasSize = canvas.GetSize();
    out << "ENGINE_UI 11\n";
    out << canvasSize.x << ' ' << canvasSize.y << '\n';

    const UIWidget* root = canvas.GetRoot();
    if (root)
        for (const auto& child : root->GetChildren())
            if (child) WriteWidget(out, *child, 0);

    return static_cast<bool>(out);
}

bool UISerializer::Load(UICanvas& canvas, const std::string& filepath)
{
    std::ifstream in(filepath);
    if (!in) return false;

    std::string magic;
    int version = 0;
    in >> magic >> version;
    // Read the old marker for existing assets, but all newly saved UI files use
    // the engine-neutral marker.
    if ((magic != "ENGINE_UI" && magic != "VORTEK_UI") || version < 1 || version > 11) return false;

    Vec2 canvasSize;
    in >> canvasSize.x >> canvasSize.y;
    if (!in) return false;

    canvas.SetSize(canvasSize);
    canvas.Clear();

    std::string line;
    std::getline(in, line);
    std::vector<UIWidget*> parents;

    while (std::getline(in, line))
    {
        if (line.empty()) continue;
        std::istringstream row(line);

        int depth = 0;
        int typeValue = 0;
        std::string name;
        Vec2 pos, size, amin, amax, pivot;
        Vec4 color;
        bool visible = true, enabled = true, hitTest = true;
        int zOrder = 0;
        bool gradient=false; Vec4 gradientColor; int gradientDirection=0;

        row >> depth >> typeValue >> std::quoted(name)
            >> pos.x >> pos.y >> size.x >> size.y
            >> amin.x >> amin.y >> amax.x >> amax.y
            >> pivot.x >> pivot.y
            >> color.x >> color.y >> color.z >> color.w
            >> visible >> enabled >> hitTest >> zOrder;
        if(version>=3) row >> gradient >> gradientColor.x >> gradientColor.y >> gradientColor.z >> gradientColor.w >> gradientDirection;
        float cornerRadius=0.0f; if(version>=9) row >> cornerRadius;
        float renderOpacity=1.0f; if(version>=10) row >> renderOpacity;

        const int maxType = version >= 11 ? static_cast<int>(UIWidgetType::ScrollBox) :
            (version >= 10 ? static_cast<int>(UIWidgetType::ProgressBar) : static_cast<int>(UIWidgetType::Slider));
        if (!row || typeValue < 0 || typeValue > maxType)
            return false;

        std::unique_ptr<UIWidget> widget =
            UIWidgetFactory::Create(static_cast<UIWidgetType>(typeValue));
        if (!widget) return false;

        widget->SetName(name);
        widget->SetPosition(pos);
        widget->SetSize(size);
        widget->SetAnchors(amin, amax);
        widget->SetPivot(pivot);
        widget->SetColor(color);
        widget->SetVisible(visible);
        widget->SetEnabled(enabled);
        widget->SetHitTestVisible(hitTest);
        widget->SetZOrder(zOrder);
        if(version>=3){widget->SetGradientEnabled(gradient);widget->SetGradientColor(gradientColor);widget->SetGradientDirection(static_cast<UIGradientDirection>(gradientDirection));}
        widget->SetCornerRadius(cornerRadius);
        widget->SetRenderOpacity(renderOpacity);

        if (UIScrollBox* scroll = dynamic_cast<UIScrollBox*>(widget.get()))
        {
            float contentHeight = 0.0f, scrollOffset = 0.0f;
            row >> contentHeight >> scrollOffset;
            if (!row) return false;
            scroll->SetContentHeight(contentHeight);
            scroll->SetScrollOffset(scrollOffset);
        }
        else if (UIText* text = dynamic_cast<UIText*>(widget.get()))
        {
            std::string value;
            float fontSize = 24.0f;
            row >> std::quoted(value) >> fontSize;
            int horizontal=0, vertical=0; if(version>=9) row >> horizontal >> vertical;
            text->SetText(DecodeText(value));
            text->SetFontSize(fontSize);
            text->SetHorizontalAlignment(static_cast<UITextHorizontalAlignment>(horizontal));
            text->SetVerticalAlignment(static_cast<UITextVerticalAlignment>(vertical));
        }
        else if (UITextInput* input = dynamic_cast<UITextInput*>(widget.get()))
        {
            if (version < 5) return false;
            std::string value, placeholder;
            float fontSize = 18.0f;
            std::size_t maxLength = 256;
            bool password = false;
            row >> std::quoted(value) >> std::quoted(placeholder) >> fontSize >> maxLength >> password;
            if (!row) return false;
            input->SetText(value);
            input->SetPlaceholder(placeholder);
            input->SetFontSize(fontSize);
            input->SetMaxLength(maxLength);
            input->SetPassword(password);
        }
        else if (UISlider* slider = dynamic_cast<UISlider*>(widget.get()))
        {
            if (version < 6) return false;
            float value=1.0f; Vec4 fill,handle;
            row >> value >> fill.x >> fill.y >> fill.z >> fill.w
                >> handle.x >> handle.y >> handle.z >> handle.w;
            if (!row) return false;
            slider->SetValue(value); slider->SetFillColor(fill); slider->SetHandleColor(handle);
        }
        else if (UIProgressBar* progress = dynamic_cast<UIProgressBar*>(widget.get()))
        {
            if (version < 10) return false;
            float percent=0.5f; Vec4 fill; int direction=0;
            row >> percent >> fill.x >> fill.y >> fill.z >> fill.w >> direction;
            if (!row || direction < 0 || direction > 3) return false;
            progress->SetPercent(percent); progress->SetFillColor(fill);
            progress->SetFillDirection(static_cast<UIProgressBarFillDirection>(direction));
        }
        else if (UIImage* image = dynamic_cast<UIImage*>(widget.get()))
        {
            std::string path;
            row >> std::quoted(path);
            image->SetTexturePath(path);
        }
        else if (UIButton* button = dynamic_cast<UIButton*>(widget.get()))
        {
            if (version >= 2)
            {
                Vec4 normal, hovered, pressed, disabled;
                row >> normal.x >> normal.y >> normal.z >> normal.w
                    >> hovered.x >> hovered.y >> hovered.z >> hovered.w
                    >> pressed.x >> pressed.y >> pressed.z >> pressed.w
                    >> disabled.x >> disabled.y >> disabled.z >> disabled.w;
                if (!row) return false;
                button->SetNormalColor(normal);
                button->SetHoveredColor(hovered);
                button->SetPressedColor(pressed);
                button->SetDisabledColor(disabled);
                if (version >= 4)
                {
                    bool affect=false; Vec4 tn,th,tp,td;
                    row >> affect >> tn.x >> tn.y >> tn.z >> tn.w >> th.x >> th.y >> th.z >> th.w >> tp.x >> tp.y >> tp.z >> tp.w >> td.x >> td.y >> td.z >> td.w;
                    if (!row) return false;
                    button->SetAffectChildText(affect); button->SetNormalTextColor(tn); button->SetHoveredTextColor(th); button->SetPressedTextColor(tp); button->SetDisabledTextColor(td);
                    if (version >= 7)
                    {
                        std::string clickSoundPath;
                        row >> std::quoted(clickSoundPath);
                        if (!row) return false;
                        button->SetClickSoundPath(clickSoundPath);
                        if (version >= 8)
                        {
                            std::string onClickScript, onClickFunction;
                            row >> std::quoted(onClickScript) >> std::quoted(onClickFunction);
                            if (!row) return false;
                            button->SetOnClickScript(onClickScript);
                            button->SetOnClickFunction(onClickFunction);
                            if (version >= 9)
                            {
                                std::string normalImage, hoveredImage, pressedImage, disabledImage;
                                row >> std::quoted(normalImage) >> std::quoted(hoveredImage) >> std::quoted(pressedImage) >> std::quoted(disabledImage);
                                if (!row) return false;
                                button->SetNormalImage(normalImage); button->SetHoveredImage(hoveredImage);
                                button->SetPressedImage(pressedImage); button->SetDisabledImage(disabledImage);
                            }
                        }
                    }
                }
            }
        }

        UIWidget* parent = depth == 0 ? canvas.GetRoot() :
            (depth - 1 < static_cast<int>(parents.size()) ? parents[depth - 1] : nullptr);
        if (!parent) return false;

        UIWidget* added = parent->AddChild(std::move(widget));
        if (static_cast<int>(parents.size()) <= depth) parents.resize(depth + 1);
        parents[depth] = added;
        parents.resize(depth + 1);
    }

    return true;
}
