#pragma once

#include "UICanvas.h"

#include <string>

class UISerializer
{
public:
    static bool Save(const UICanvas& canvas, const std::string& filepath);
    static bool Load(UICanvas& canvas, const std::string& filepath,
                     const std::string& referenceHost = {});
    // Resolve and instantiate a reusable .ui asset relative to its host UI.
    static bool PopulateUserWidget(class UIUserWidget& widget, const std::string& hostPath);
};
