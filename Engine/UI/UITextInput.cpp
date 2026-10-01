#include "UITextInput.h"
#include <algorithm>

UITextInput::UITextInput() : UIWidget(UIWidgetType::TextInput)
{
    SetName("TextInput");
    SetColor(Vec4(0.08f, 0.09f, 0.11f, 1.0f));
}
void UITextInput::SetText(const std::string& value)
{
    m_Text = value.substr(0, m_MaxLength);
    m_Cursor = m_Text.size();
    m_SelectAll = false;
}
void UITextInput::SetMaxLength(std::size_t value)
{
    m_MaxLength = std::max<std::size_t>(1, value);
    if (m_Text.size() > m_MaxLength) m_Text.resize(m_MaxLength);
    m_Cursor = std::min(m_Cursor, m_Text.size());
}
void UITextInput::SetCursor(std::size_t value)
{
    m_Cursor = std::min(value, m_Text.size());
    m_SelectAll = false;
}
void UITextInput::Insert(const std::string& value)
{
    if (m_SelectAll) { m_Text.clear(); m_Cursor = 0; m_SelectAll = false; }
    const std::size_t room = m_MaxLength > m_Text.size() ? m_MaxLength - m_Text.size() : 0;
    if (!room) return;
    const std::string clipped = value.substr(0, room);
    m_Text.insert(m_Cursor, clipped);
    m_Cursor += clipped.size();
}
void UITextInput::Backspace()
{
    if (m_SelectAll) { m_Text.clear(); m_Cursor = 0; m_SelectAll = false; return; }
    if (m_Cursor == 0) return;
    m_Text.erase(m_Cursor - 1, 1); --m_Cursor;
}
void UITextInput::DeleteForward()
{
    if (m_SelectAll) { m_Text.clear(); m_Cursor = 0; m_SelectAll = false; return; }
    if (m_Cursor < m_Text.size()) m_Text.erase(m_Cursor, 1);
}
void UITextInput::MoveCursorLeft()
{
    if (m_SelectAll) { m_Cursor = 0; m_SelectAll = false; return; }
    if (m_Cursor) --m_Cursor;
}
void UITextInput::MoveCursorRight()
{
    if (m_SelectAll) { m_Cursor = m_Text.size(); m_SelectAll = false; return; }
    if (m_Cursor < m_Text.size()) ++m_Cursor;
}
std::string UITextInput::GetDisplayText() const
{
    if (m_Text.empty()) return m_Placeholder;
    return m_Password ? std::string(m_Text.size(), '*') : m_Text;
}
