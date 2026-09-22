#include "UIWidgetFactory.h"

#include "UIPanel.h"
#include "UIText.h"
#include "UIImage.h"
#include "UIButton.h"

std::unique_ptr<UIWidget>
UIWidgetFactory::Create(UIWidgetType type)
{
    switch (type)
    {
    case UIWidgetType::Panel:
        return std::make_unique<UIPanel>();

    case UIWidgetType::Text:
        return std::make_unique<UIText>();

    case UIWidgetType::Image:
        return std::make_unique<UIImage>();

    case UIWidgetType::Button:
        return std::make_unique<UIButton>();
    }

    return nullptr;
}