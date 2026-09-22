#include "UILayout.h"

UIRect UILayout::Calculate(
    const UIWidget& widget,
    const UIRect& parentRect)
{
    const Vec2 position =
        widget.GetPosition();

    const Vec2 size =
        widget.GetSize();

    const Vec2 anchor =
        widget.GetAnchor();

    const Vec2 pivot =
        widget.GetPivot();

    const float anchorX =
        parentRect.x +
        parentRect.width * anchor.x;

    const float anchorY =
        parentRect.y +
        parentRect.height * anchor.y;

    UIRect result;

    result.width = size.x;
    result.height = size.y;

    result.x =
        anchorX +
        position.x -
        size.x * pivot.x;

    result.y =
        anchorY +
        position.y -
        size.y * pivot.y;

    return result;
}