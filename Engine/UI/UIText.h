#pragma once

#include "UIWidget.h"

#include <string>

enum class UITextHorizontalAlignment { Left, Center, Right };
enum class UITextVerticalAlignment { Top, Center, Bottom };

class UIText : public UIWidget
{
public:
    UIText();

    const std::string& GetText() const;
    void SetText(const std::string& text);

    float GetFontSize() const;
    void SetFontSize(float size);
    UITextHorizontalAlignment GetHorizontalAlignment() const { return m_HorizontalAlignment; }
    void SetHorizontalAlignment(UITextHorizontalAlignment value) { m_HorizontalAlignment = value; }
    UITextVerticalAlignment GetVerticalAlignment() const { return m_VerticalAlignment; }
    void SetVerticalAlignment(UITextVerticalAlignment value) { m_VerticalAlignment = value; }

private:
    std::string m_Text = "Text";
    float m_FontSize = 24.0f;
    UITextHorizontalAlignment m_HorizontalAlignment = UITextHorizontalAlignment::Left;
    UITextVerticalAlignment m_VerticalAlignment = UITextVerticalAlignment::Top;
};