#pragma once

#include <memory>
#include <string>
#include <vector>

struct Vec2
{
    float x = 0.0f;
    float y = 0.0f;

    Vec2() = default;
    Vec2(float xValue, float yValue) : x(xValue), y(yValue) {}
};

struct Vec4
{
    float x = 1.0f;
    float y = 1.0f;
    float z = 1.0f;
    float w = 1.0f;

    Vec4() = default;
    Vec4(float xValue, float yValue, float zValue, float wValue)
        : x(xValue), y(yValue), z(zValue), w(wValue) {}
};

struct UIAnchors
{
    Vec2 minimum = Vec2(0.0f, 0.0f);
    Vec2 maximum = Vec2(0.0f, 0.0f);
};

struct UIOffsets
{
    float left = 0.0f;
    float top = 0.0f;
    float right = 100.0f;
    float bottom = 100.0f;
};

enum class UIGradientDirection { Vertical, Horizontal };

enum class UIWidgetType
{
    Panel,
    Text,
    Image,
    Button
};

class UIWidget
{
public:
    explicit UIWidget(UIWidgetType type);
    virtual ~UIWidget() = default;

    UIWidgetType GetType() const;

    const std::string& GetName() const;
    void SetName(const std::string& name);

    Vec2 GetPosition() const;
    void SetPosition(const Vec2& position);

    Vec2 GetSize() const;
    void SetSize(const Vec2& size);

    Vec2 GetAnchor() const;
    void SetAnchor(const Vec2& anchor);

    Vec2 GetAnchorMinimum() const;
    Vec2 GetAnchorMaximum() const;
    void SetAnchors(const Vec2& minimum, const Vec2& maximum);
    bool IsStretched() const;

    Vec2 GetPivot() const;
    void SetPivot(const Vec2& pivot);

    const Vec4& GetColor() const;
    void SetColor(const Vec4& color);
    bool HasGradient() const { return m_GradientEnabled; }
    void SetGradientEnabled(bool enabled) { m_GradientEnabled = enabled; }
    const Vec4& GetGradientColor() const { return m_GradientColor; }
    void SetGradientColor(const Vec4& color) { m_GradientColor = color; }
    UIGradientDirection GetGradientDirection() const { return m_GradientDirection; }
    void SetGradientDirection(UIGradientDirection direction) { m_GradientDirection = direction; }

    bool IsVisible() const;
    void SetVisible(bool visible);

    bool IsEnabled() const;
    void SetEnabled(bool enabled);

    bool IsHitTestVisible() const;
    void SetHitTestVisible(bool enabled);

    int GetZOrder() const;
    void SetZOrder(int zOrder);

    UIWidget* GetParent() const;
    const std::vector<std::unique_ptr<UIWidget>>& GetChildren() const;

    UIWidget* AddChild(std::unique_ptr<UIWidget> child);
    std::unique_ptr<UIWidget> DetachChild(UIWidget* child);
    bool IsDescendantOf(const UIWidget* widget) const;
    void RemoveChild(UIWidget* child);
    UIWidget* Find(const std::string& name);
    const UIWidget* Find(const std::string& name) const;

private:
    UIWidgetType m_Type;
    std::string m_Name;

    UIAnchors m_Anchors;
    UIOffsets m_Offsets;
    Vec2 m_Pivot = Vec2(0.0f, 0.0f);
    Vec4 m_Color;
    bool m_GradientEnabled = false;
    Vec4 m_GradientColor;
    UIGradientDirection m_GradientDirection = UIGradientDirection::Vertical;

    bool m_Visible = true;
    bool m_Enabled = true;
    bool m_HitTestVisible = true;
    int m_ZOrder = 0;

    UIWidget* m_Parent = nullptr;
    std::vector<std::unique_ptr<UIWidget>> m_Children;
};
