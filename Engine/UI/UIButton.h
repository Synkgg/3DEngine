#pragma once

#include "UIWidget.h"

class UIButton : public UIWidget
{
public:
    UIButton();

    bool IsHovered() const;
    void SetHovered(bool hovered);

    bool IsPressed() const;
    void SetPressed(bool pressed);

    bool WasClicked() const;
    void SetClicked(bool clicked);
    bool ConsumeClick();

private:
    bool m_Hovered = false;
    bool m_Pressed = false;
    bool m_Clicked = false;
};
