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

private:
    bool m_Hovered = false;
    bool m_Pressed = false;
};