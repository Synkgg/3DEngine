#pragma once

#include <memory>
#include <string>
#include <vector>

struct Vec2
{
    float x = 0.0f;
    float y = 0.0f;
};

struct Vec4
{
    float x = 1.0f;
    float y = 1.0f;
    float z = 1.0f;
    float w = 1.0f;
};

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

    Vec2 GetPivot() const;
    void SetPivot(const Vec2& pivot);

    const Vec4& GetColor() const;
    void SetColor(const Vec4& color);

    bool IsVisible() const;
    void SetVisible(bool visible);

    UIWidget* GetParent() const;

    const std::vector<std::unique_ptr<UIWidget>>&
        GetChildren() const;

    UIWidget* AddChild(
        std::unique_ptr<UIWidget> child);

    void RemoveChild(UIWidget* child);

private:
    UIWidgetType m_Type;

    std::string m_Name;

    Vec2 m_Position =
        Vec2(0.0f, 0.0f);

    Vec2 m_Size =
        Vec2(100.0f, 100.0f);

    Vec2 m_Anchor =
        Vec2(0.0f, 0.0f);

    Vec2 m_Pivot =
        Vec2(0.0f, 0.0f);

    Vec4 m_Color;

    bool m_Visible = true;

    UIWidget* m_Parent = nullptr;

    std::vector<std::unique_ptr<UIWidget>> m_Children;
};