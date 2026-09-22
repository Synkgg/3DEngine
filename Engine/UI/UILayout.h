#pragma once

#include "UIWidget.h"

struct UIRect
{
    float x = 0.0f;
    float y = 0.0f;

    float width = 0.0f;
    float height = 0.0f;
};

class UILayout
{
public:
    static UIRect Calculate(
        const UIWidget& widget,
        const UIRect& parentRect);
};