#include "UISerializer.h"

#include "UIButton.h"
#include "UIImage.h"
#include "UIText.h"
#include "UIWidgetFactory.h"

#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>

namespace
{
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
            << widget.IsHitTestVisible() << ' ' << widget.GetZOrder();

        if (const UIText* text = dynamic_cast<const UIText*>(&widget))
            out << ' ' << std::quoted(text->GetText()) << ' ' << text->GetFontSize();
        else if (const UIImage* image = dynamic_cast<const UIImage*>(&widget))
            out << ' ' << std::quoted(image->GetTexturePath());

        out << '\n';

        for (const auto& child : widget.GetChildren())
            if (child) WriteWidget(out, *child, depth + 1);
    }
}

bool UISerializer::Save(const UICanvas& canvas, const std::string& filepath)
{
    std::ofstream out(filepath);
    if (!out) return false;

    const Vec2 canvasSize = canvas.GetSize();
    out << "VORTEK_UI 1\n";
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
    if (magic != "VORTEK_UI" || version != 1) return false;

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

        row >> depth >> typeValue >> std::quoted(name)
            >> pos.x >> pos.y >> size.x >> size.y
            >> amin.x >> amin.y >> amax.x >> amax.y
            >> pivot.x >> pivot.y
            >> color.x >> color.y >> color.z >> color.w
            >> visible >> enabled >> hitTest >> zOrder;

        if (!row || typeValue < 0 || typeValue > static_cast<int>(UIWidgetType::Button))
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

        if (UIText* text = dynamic_cast<UIText*>(widget.get()))
        {
            std::string value;
            float fontSize = 24.0f;
            row >> std::quoted(value) >> fontSize;
            text->SetText(value);
            text->SetFontSize(fontSize);
        }
        else if (UIImage* image = dynamic_cast<UIImage*>(widget.get()))
        {
            std::string path;
            row >> std::quoted(path);
            image->SetTexturePath(path);
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
