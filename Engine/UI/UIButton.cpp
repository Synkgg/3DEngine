#include "UIButton.h"

UIButton::UIButton()
    : UIWidget(UIWidgetType::Button)
{
    SetName("Button");
}

bool UIButton::IsHovered() const { return m_Hovered; }
void UIButton::SetHovered(bool hovered) { m_Hovered = hovered; }

bool UIButton::IsPressed() const { return m_Pressed; }
void UIButton::SetPressed(bool pressed) { m_Pressed = pressed; }

bool UIButton::WasClicked() const { return m_Clicked; }
void UIButton::SetClicked(bool clicked) { m_Clicked = clicked; }

bool UIButton::ConsumeClick()
{
    const bool clicked = m_Clicked;
    m_Clicked = false;
    return clicked;
}

const Vec4& UIButton::GetNormalColor() const { return m_NormalColor; }
void UIButton::SetNormalColor(const Vec4& color) { m_NormalColor = color; }

const Vec4& UIButton::GetHoveredColor() const { return m_HoveredColor; }
void UIButton::SetHoveredColor(const Vec4& color) { m_HoveredColor = color; }

const Vec4& UIButton::GetPressedColor() const { return m_PressedColor; }
void UIButton::SetPressedColor(const Vec4& color) { m_PressedColor = color; }

const Vec4& UIButton::GetDisabledColor() const { return m_DisabledColor; }
void UIButton::SetDisabledColor(const Vec4& color) { m_DisabledColor = color; }

Vec4 UIButton::GetCurrentColor() const
{
    if (!IsEnabled()) return m_DisabledColor;
    if (m_Pressed) return m_PressedColor;
    if (m_Hovered) return m_HoveredColor;
    return m_NormalColor;
}

Vec4 UIButton::GetCurrentTextColor() const
{
    if (!IsEnabled()) return m_DisabledTextColor;
    if (m_Pressed) return m_PressedTextColor;
    if (m_Hovered) return m_HoveredTextColor;
    return m_NormalTextColor;
}
