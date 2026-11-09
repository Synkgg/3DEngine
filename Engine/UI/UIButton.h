#pragma once

#include "UIWidget.h"
#include <string>

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

    const std::string& GetOnClickScript() const { return m_OnClickScript; }
    void SetOnClickScript(const std::string& value) { m_OnClickScript = value; }
    const std::string& GetOnClickFunction() const { return m_OnClickFunction; }
    void SetOnClickFunction(const std::string& value) { m_OnClickFunction = value; }
    bool ConsumeClickEvent();

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

    const std::string& GetNormalImage() const { return m_NormalImage; }
    const std::string& GetHoveredImage() const { return m_HoveredImage; }
    const std::string& GetPressedImage() const { return m_PressedImage; }
    const std::string& GetDisabledImage() const { return m_DisabledImage; }
    void SetNormalImage(const std::string& v) { m_NormalImage=v; }
    void SetHoveredImage(const std::string& v) { m_HoveredImage=v; }
    void SetPressedImage(const std::string& v) { m_PressedImage=v; }
    void SetDisabledImage(const std::string& v) { m_DisabledImage=v; }
    const std::string& GetCurrentImage() const {
        if (!IsEnabledInHierarchy() && !m_DisabledImage.empty()) return m_DisabledImage;
        if (IsEnabledInHierarchy() && m_Pressed && !m_PressedImage.empty()) return m_PressedImage;
        if (IsEnabledInHierarchy() && m_Hovered && !m_HoveredImage.empty()) return m_HoveredImage;
        return m_NormalImage;
    }

    const std::string& GetClickSoundPath() const { return m_ClickSoundPath; }
    void SetClickSoundPath(const std::string& path) { m_ClickSoundPath = path; }

private:
    bool m_Hovered = false;
    bool m_Pressed = false;
    bool m_Clicked = false;
    bool m_ClickEventPending = false;
    std::string m_OnClickScript;
    std::string m_OnClickFunction;

    Vec4 m_NormalColor = Vec4(0.22f, 0.24f, 0.28f, 1.0f);
    Vec4 m_HoveredColor = Vec4(0.30f, 0.34f, 0.40f, 1.0f);
    Vec4 m_PressedColor = Vec4(0.12f, 0.45f, 0.66f, 1.0f);
    Vec4 m_DisabledColor = Vec4(0.16f, 0.17f, 0.19f, 0.55f);
    bool m_AffectChildText = false;
    std::string m_ClickSoundPath;
    std::string m_NormalImage, m_HoveredImage, m_PressedImage, m_DisabledImage;
    Vec4 m_NormalTextColor = Vec4(0.8f,0.8f,0.8f,1.0f), m_HoveredTextColor = Vec4(1,1,1,1), m_PressedTextColor = Vec4(1,1,1,1), m_DisabledTextColor = Vec4(0.5f,0.5f,0.5f,0.6f);
};
