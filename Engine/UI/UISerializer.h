#pragma once

#include "UICanvas.h"

#include <string>

class UISerializer
{
public:
    static bool Save(const UICanvas& canvas, const std::string& filepath);
    static bool Load(UICanvas& canvas, const std::string& filepath);
};
