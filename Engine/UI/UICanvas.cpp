#include "UICanvas.h"

#include "UIPanel.h"

UICanvas::UICanvas()
    : m_Size(1920.0f, 1080.0f)
{
    Clear();
}

UIWidget* UICanvas::GetRoot() { return m_Root.get(); }
const UIWidget* UICanvas::GetRoot() const { return m_Root.get(); }

void UICanvas::SetSize(const Vec2& size)
{
    m_Size = size;
    if (m_Root) m_Root->SetSize(size);
}

Vec2 UICanvas::GetSize() const { return m_Size; }

void UICanvas::Clear()
{
    m_Root = std::make_unique<UIPanel>();
    m_Root->SetName("Canvas");
    m_Root->SetPosition(Vec2(0.0f, 0.0f));
    m_Root->SetSize(m_Size);
    m_Root->SetHitTestVisible(false);
}
