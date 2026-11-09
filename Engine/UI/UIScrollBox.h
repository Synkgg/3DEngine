#pragma once

#include "UIWidget.h"
#include <algorithm>

// A clipped, vertically scrollable container. Children retain their normal
// anchors and local positions; scroll moves their shared content origin.
class UIScrollBox final : public UIWidget
{
public:
    UIScrollBox() : UIWidget(UIWidgetType::ScrollBox)
    {
        SetName("Scroll Box");
        SetSize(Vec2(420.0f, 300.0f));
        SetColor(Vec4(0.08f, 0.10f, 0.12f, 0.85f));
        SetHitTestVisible(false);
    }

    float GetScrollOffset() const { return m_ScrollOffset; }
    void SetScrollOffset(float offset) { m_ScrollOffset = std::max(0.0f, offset); }

    // Optional minimum content height for layouts with sparse children.
    float GetContentHeight() const { return m_ContentHeight; }
    void SetContentHeight(float height) { m_ContentHeight = std::max(0.0f, height); }

private:
    float m_ScrollOffset = 0.0f;
    float m_ContentHeight = 0.0f;
};
