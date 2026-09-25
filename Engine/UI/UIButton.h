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

    const Vec4& GetNormalColor() const;
    void SetNormalColor(const Vec4& color);

    const Vec4& GetHoveredColor() const;
    void SetHoveredColor(const Vec4& color);

    const Vec4& GetPressedColor() const;
    void SetPressedColor(const Vec4& color);

    const Vec4& GetDisabledColor() const;
    void SetDisabledColor(const Vec4& color);

    Vec4 GetCurrentColor() const;

private:
    bool m_Hovered = false;
    bool m_Pressed = false;
    bool m_Clicked = false;

    Vec4 m_NormalColor = Vec4(0.22f, 0.24f, 0.28f, 1.0f);
    Vec4 m_HoveredColor = Vec4(0.30f, 0.34f, 0.40f, 1.0f);
    Vec4 m_PressedColor = Vec4(0.12f, 0.45f, 0.66f, 1.0f);
    Vec4 m_DisabledColor = Vec4(0.16f, 0.17f, 0.19f, 0.55f);
};
