#include "UIWidgetFactory.h"

#include "UIPanel.h"
#include "UIText.h"
#include "UIImage.h"
#include "UIButton.h"
#include "UITextInput.h"
#include "UISlider.h"

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

    case UIWidgetType::TextInput:
        return std::make_unique<UITextInput>();

    case UIWidgetType::Slider:
        return std::make_unique<UISlider>();
    }

    return nullptr;
}