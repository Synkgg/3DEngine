#include "UIButton.h"

UIButton::UIButton()
    : UIWidget(UIWidgetType::Button)
{
    SetName("Button");
}

bool UIButton::IsHovered() const
{
    return m_Hovered;
}

void UIButton::SetHovered(bool hovered)
{
    m_Hovered = hovered;
}

bool UIButton::IsPressed() const
{
    return m_Pressed;
}

void UIButton::SetPressed(bool pressed)
{
    m_Pressed = pressed;
}