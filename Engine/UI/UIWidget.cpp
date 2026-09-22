#include "UIWidget.h"

#include <algorithm>

UIWidget::UIWidget(UIWidgetType type)
    : m_Type(type)
{
}

UIWidgetType UIWidget::GetType() const
{
    return m_Type;
}

const std::string& UIWidget::GetName() const
{
    return m_Name;
}

void UIWidget::SetName(
    const std::string& name)
{
    m_Name = name;
}

Vec2 UIWidget::GetPosition() const
{
    return m_Position;
}

void UIWidget::SetPosition(
    const Vec2& position)
{
    m_Position = position;
}

Vec2 UIWidget::GetSize() const
{
    return m_Size;
}

void UIWidget::SetSize(
    const Vec2& size)
{
    m_Size = size;
}

Vec2 UIWidget::GetAnchor() const
{
    return m_Anchor;
}

void UIWidget::SetAnchor(
    const Vec2& anchor)
{
    m_Anchor = anchor;
}

Vec2 UIWidget::GetPivot() const
{
    return m_Pivot;
}

void UIWidget::SetPivot(
    const Vec2& pivot)
{
    m_Pivot = pivot;
}

const Vec4& UIWidget::GetColor() const
{
    return m_Color;
}

void UIWidget::SetColor(
    const Vec4& color)
{
    m_Color = color;
}

bool UIWidget::IsVisible() const
{
    return m_Visible;
}

void UIWidget::SetVisible(
    bool visible)
{
    m_Visible = visible;
}

UIWidget* UIWidget::GetParent() const
{
    return m_Parent;
}

const std::vector<std::unique_ptr<UIWidget>>&
UIWidget::GetChildren() const
{
    return m_Children;
}

UIWidget* UIWidget::AddChild(
    std::unique_ptr<UIWidget> child)
{
    if (!child)
    {
        return nullptr;
    }

    child->m_Parent = this;

    UIWidget* widget = child.get();

    m_Children.push_back(
        std::move(child)
    );

    return widget;
}

void UIWidget::RemoveChild(
    UIWidget* child)
{
    if (!child)
    {
        return;
    }

    auto it = std::find_if(
        m_Children.begin(),
        m_Children.end(),
        [child](
            const std::unique_ptr<UIWidget>& widget)
        {
            return widget.get() == child;
        }
    );

    if (it == m_Children.end())
    {
        return;
    }

    (*it)->m_Parent = nullptr;

    m_Children.erase(it);
}