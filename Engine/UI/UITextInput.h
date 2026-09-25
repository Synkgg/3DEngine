#pragma once
#include "UIWidget.h"
#include <string>

class UITextInput : public UIWidget
{
public:
    UITextInput();

    const std::string& GetText() const { return m_Text; }
    void SetText(const std::string& value);
    const std::string& GetPlaceholder() const { return m_Placeholder; }
    void SetPlaceholder(const std::string& value) { m_Placeholder = value; }
    float GetFontSize() const { return m_FontSize; }
    void SetFontSize(float value) { m_FontSize = value; }
    std::size_t GetMaxLength() const { return m_MaxLength; }
    void SetMaxLength(std::size_t value);
    bool IsFocused() const { return m_Focused; }
    void SetFocused(bool value) { m_Focused = value; if (!value) m_SelectAll = false; }
    bool IsPassword() const { return m_Password; }
    void SetPassword(bool value) { m_Password = value; }
    std::size_t GetCursor() const { return m_Cursor; }
    void SetCursor(std::size_t value);
    bool HasSelection() const { return m_SelectAll && !m_Text.empty(); }
    void SelectAll() { m_SelectAll = true; m_Cursor = m_Text.size(); }
    void ClearSelection() { m_SelectAll = false; }
    void Insert(const std::string& value);
    void Backspace();
    void DeleteForward();
    void MoveCursorLeft();
    void MoveCursorRight();
    std::string GetDisplayText() const;

private:
    std::string m_Text;
    std::string m_Placeholder = "Enter text...";
    float m_FontSize = 18.0f;
    std::size_t m_MaxLength = 256;
    std::size_t m_Cursor = 0;
    bool m_Focused = false;
    bool m_Password = false;
    bool m_SelectAll = false;
};
