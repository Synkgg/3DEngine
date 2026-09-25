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
    bool GetAffectChildText() const { return m_AffectChildText; }
    void SetAffectChildText(bool value) { m_AffectChildText = value; }
    const Vec4& GetNormalTextColor() const { return m_NormalTextColor; }
    const Vec4& GetHoveredTextColor() const { return m_HoveredTextColor; }
    const Vec4& GetPressedTextColor() const { return m_PressedTextColor; }
    const Vec4& GetDisabledTextColor() const { return m_DisabledTextColor; }
    void SetNormalTextColor(const Vec4& v){m_NormalTextColor=v;} void SetHoveredTextColor(const Vec4& v){m_HoveredTextColor=v;} void SetPressedTextColor(const Vec4& v){m_PressedTextColor=v;} void SetDisabledTextColor(const Vec4& v){m_DisabledTextColor=v;}
    Vec4 GetCurrentTextColor() const;

private:
    bool m_Hovered = false;
    bool m_Pressed = false;
    bool m_Clicked = false;

    Vec4 m_NormalColor = Vec4(0.22f, 0.24f, 0.28f, 1.0f);
    Vec4 m_HoveredColor = Vec4(0.30f, 0.34f, 0.40f, 1.0f);
    Vec4 m_PressedColor = Vec4(0.12f, 0.45f, 0.66f, 1.0f);
    Vec4 m_DisabledColor = Vec4(0.16f, 0.17f, 0.19f, 0.55f);
    bool m_AffectChildText = false;
    Vec4 m_NormalTextColor = Vec4(0.8f,0.8f,0.8f,1.0f), m_HoveredTextColor = Vec4(1,1,1,1), m_PressedTextColor = Vec4(1,1,1,1), m_DisabledTextColor = Vec4(0.5f,0.5f,0.5f,0.6f);
};
