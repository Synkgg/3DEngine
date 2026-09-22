#pragma once

#include "UIWidget.h"

#include <memory>

class UICanvas
{
public:
    UICanvas();

    UIWidget* GetRoot();
    const UIWidget* GetRoot() const;

    void SetSize(const Vec2& size);
    Vec2 GetSize() const;

    void Clear();

private:
    Vec2 m_Size;

    std::unique_ptr<UIWidget> m_Root;
};