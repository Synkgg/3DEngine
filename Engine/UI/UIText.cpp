#include "UIText.h"

UIText::UIText()
    : UIWidget(UIWidgetType::Text)
{
    SetName("Text");
}

const std::string& UIText::GetText() const
{
    return m_Text;
}

void UIText::SetText(
    const std::string& text)
{
    m_Text = text;
}

float UIText::GetFontSize() const
{
    return m_FontSize;
}

void UIText::SetFontSize(float size)
{
    m_FontSize = size;
}