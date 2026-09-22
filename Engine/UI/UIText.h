#pragma once

#include "UIWidget.h"

#include <string>

class UIText : public UIWidget
{
public:
    UIText();

    const std::string& GetText() const;
    void SetText(const std::string& text);

    float GetFontSize() const;
    void SetFontSize(float size);

private:
    std::string m_Text = "Text";
    float m_FontSize = 24.0f;
};