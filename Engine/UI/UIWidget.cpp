#include "UIWidget.h"

#include <algorithm>
#include <atomic>

UIWidget::UIWidget(UIWidgetType type)
    : m_Type(type)
{
    static std::atomic<std::uint64_t> nextId{1};
    m_InstanceId = nextId.fetch_add(1, std::memory_order_relaxed);
}

UIWidgetType UIWidget::GetType() const { return m_Type; }

bool UIWidget::IsEnabledInHierarchy() const
{
    for (const UIWidget* widget = this; widget; widget = widget->GetParent())
        if (!widget->IsEnabled()) return false;
    return true;
}

const std::string& UIWidget::GetName() const { return m_Name; }
void UIWidget::SetName(const std::string& name) { m_Name = name; }

Vec2 UIWidget::GetPosition() const
{
    return Vec2(m_Offsets.left, m_Offsets.top);
}

void UIWidget::SetPosition(const Vec2& position)
{
    m_Offsets.left = position.x;
    m_Offsets.top = position.y;
}

Vec2 UIWidget::GetSize() const
{
    return Vec2(m_Offsets.right, m_Offsets.bottom);
}

void UIWidget::SetSize(const Vec2& size)
{
    m_Offsets.right = std::max(0.0f, size.x);
    m_Offsets.bottom = std::max(0.0f, size.y);
}

Vec2 UIWidget::GetAnchor() const { return m_Anchors.minimum; }

void UIWidget::SetAnchor(const Vec2& anchor)
{
    m_Anchors.minimum = anchor;
    m_Anchors.maximum = anchor;
}

Vec2 UIWidget::GetAnchorMinimum() const { return m_Anchors.minimum; }
Vec2 UIWidget::GetAnchorMaximum() const { return m_Anchors.maximum; }

void UIWidget::SetAnchors(const Vec2& minimum, const Vec2& maximum)
{
    m_Anchors.minimum = minimum;
    m_Anchors.maximum = maximum;
}

bool UIWidget::IsStretched() const
{
    return m_Anchors.minimum.x != m_Anchors.maximum.x ||
           m_Anchors.minimum.y != m_Anchors.maximum.y;
}

Vec2 UIWidget::GetPivot() const { return m_Pivot; }
void UIWidget::SetPivot(const Vec2& pivot) { m_Pivot = pivot; }

const Vec4& UIWidget::GetColor() const { return m_Color; }
void UIWidget::SetColor(const Vec4& color) { m_Color = color; }

bool UIWidget::IsVisible() const { return m_Visible; }
void UIWidget::SetVisible(bool visible) { m_Visible = visible; }

bool UIWidget::IsEnabled() const { return m_Enabled; }
void UIWidget::SetEnabled(bool enabled) { m_Enabled = enabled; }

bool UIWidget::IsHitTestVisible() const { return m_HitTestVisible; }
void UIWidget::SetHitTestVisible(bool enabled) { m_HitTestVisible = enabled; }

int UIWidget::GetZOrder() const { return m_ZOrder; }
void UIWidget::SetZOrder(int zOrder) {
    m_ZOrder = zOrder;
    if (m_Parent) std::stable_sort(m_Parent->m_Children.begin(), m_Parent->m_Children.end(),
        [](const auto& a, const auto& b) { return a->GetZOrder() < b->GetZOrder(); });
}

float UIWidget::GetRenderOpacity() const { return m_RenderOpacity; }
void UIWidget::SetRenderOpacity(float opacity) { m_RenderOpacity = std::clamp(opacity, 0.0f, 1.0f); }

UIWidget* UIWidget::GetParent() const { return m_Parent; }

const std::vector<std::unique_ptr<UIWidget>>& UIWidget::GetChildren() const
{
    return m_Children;
}

UIWidget* UIWidget::AddChild(std::unique_ptr<UIWidget> child)
{
    if (!child) return nullptr;

    child->m_Parent = this;
    UIWidget* result = child.get();
    m_Children.push_back(std::move(child));

    std::stable_sort(
        m_Children.begin(),
        m_Children.end(),
        [](const std::unique_ptr<UIWidget>& a, const std::unique_ptr<UIWidget>& b)
        {
            return a->GetZOrder() < b->GetZOrder();
        });

    return result;
}

std::unique_ptr<UIWidget> UIWidget::DetachChild(UIWidget* child)
{
    if (!child) return nullptr;
    auto it = std::find_if(m_Children.begin(), m_Children.end(),
        [child](const std::unique_ptr<UIWidget>& widget) { return widget.get() == child; });
    if (it == m_Children.end()) return nullptr;
    std::unique_ptr<UIWidget> detached = std::move(*it);
    m_Children.erase(it);
    detached->m_Parent = nullptr;
    return detached;
}

bool UIWidget::IsDescendantOf(const UIWidget* widget) const
{
    if (!widget) return false;
    for (const UIWidget* parent = m_Parent; parent; parent = parent->m_Parent)
        if (parent == widget) return true;
    return false;
}

void UIWidget::RemoveChild(UIWidget* child)
{
    if (!child) return;

    auto it = std::find_if(
        m_Children.begin(),
        m_Children.end(),
        [child](const std::unique_ptr<UIWidget>& widget)
        {
            return widget.get() == child;
        });

    if (it == m_Children.end()) return;
    (*it)->m_Parent = nullptr;
    m_Children.erase(it);
}

UIWidget* UIWidget::Find(const std::string& name)
{
    return const_cast<UIWidget*>(
        static_cast<const UIWidget*>(this)->Find(name));
}

const UIWidget* UIWidget::Find(const std::string& name) const
{
    // Qualified paths disambiguate multiple instances of the same source
    // widget (e.g. "Inventory.HealthBar"). Unqualified legacy names still
    // search the entire hierarchy as before.
    const std::size_t separator = name.find('.');
    if (separator != std::string::npos)
    {
        const std::string first = name.substr(0, separator);
        if (m_Name == first)
        {
            const UIWidget* node = this;
            std::size_t start = separator + 1;
            while (start < name.size())
            {
                const std::size_t end = name.find('.', start);
                const std::string segment = name.substr(start, end == std::string::npos ?
                    std::string::npos : end - start);
                if (segment.empty()) return nullptr;
                const UIWidget* next = nullptr;
                for (const auto& child : node->GetChildren())
                    if (child->GetName() == segment) { next = child.get(); break; }
                if (!next) return nullptr;
                node = next;
                if (end == std::string::npos) return node;
                start = end + 1;
            }
            return nullptr;
        }
        for (const auto& child : m_Children)
            if (const UIWidget* result = child->Find(name)) return result;
        return nullptr;
    }

    if (m_Name == name) return this;
    for (const auto& child : m_Children)
        if (const UIWidget* result = child->Find(name)) return result;
    return nullptr;
}
