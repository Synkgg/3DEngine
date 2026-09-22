#include "UILayout.h"

#include <algorithm>

UIRect UILayout::Calculate(
    const UIWidget& widget,
    const UIRect& parentRect)
{
    const Vec2 anchorMin = widget.GetAnchorMinimum();
    const Vec2 anchorMax = widget.GetAnchorMaximum();
    const Vec2 position = widget.GetPosition();
    const Vec2 size = widget.GetSize();
    const Vec2 pivot = widget.GetPivot();

    const float minX = parentRect.x + parentRect.width * anchorMin.x;
    const float minY = parentRect.y + parentRect.height * anchorMin.y;
    const float maxX = parentRect.x + parentRect.width * anchorMax.x;
    const float maxY = parentRect.y + parentRect.height * anchorMax.y;

    UIRect result;

    if (anchorMin.x == anchorMax.x)
    {
        result.width = size.x;
        result.x = minX + position.x - result.width * pivot.x;
    }
    else
    {
        result.x = minX + position.x;
        result.width = std::max(0.0f, (maxX - minX) - position.x - size.x);
    }

    if (anchorMin.y == anchorMax.y)
    {
        result.height = size.y;
        result.y = minY + position.y - result.height * pivot.y;
    }
    else
    {
        result.y = minY + position.y;
        result.height = std::max(0.0f, (maxY - minY) - position.y - size.y);
    }

    return result;
}
