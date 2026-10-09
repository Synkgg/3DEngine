#pragma once

#include "UIWidget.h"

#include <memory>
#include <cstdint>

class UICanvas
{
public:
    UICanvas();

    UIWidget* GetRoot();
    const UIWidget* GetRoot() const;

    void SetSize(const Vec2& size);
    Vec2 GetSize() const;

    void Clear();
    std::uint64_t GetRevision() const { return m_Revision; }

private:
    Vec2 m_Size;
    std::uint64_t m_Revision = 0;

    std::unique_ptr<UIWidget> m_Root;
};
