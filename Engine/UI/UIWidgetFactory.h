#pragma once

#include "UIWidget.h"

#include <memory>

class UIWidgetFactory
{
public:
    static std::unique_ptr<UIWidget> Create(
        UIWidgetType type);
};