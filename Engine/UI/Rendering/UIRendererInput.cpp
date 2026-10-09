#include "../UIRenderer.h"
#include "../UIText.h"
#include "../UIImage.h"
#include "../UIButton.h"
#include "../UITextInput.h"
#include "../UISlider.h"
#include "../UIScrollBox.h"
#include "../../Platform/SDL/Input.h"
#include "../../Graphics/Renderer.h"
#include "../../Graphics/Texture2D.h"
#include "../../Audio/AudioEngine.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <string>

bool UIRenderer::ViewportToCanvas(
    float mouseX,
    float mouseY,
    float viewportWidth,
    float viewportHeight,
    Vec2& result) const
{
    if (viewportWidth <= 0.0f || viewportHeight <= 0.0f ||
        m_LogicalWidth <= 0.0f || m_LogicalHeight <= 0.0f)
        return false;

    const float scale = std::min(
        viewportWidth / m_LogicalWidth,
        viewportHeight / m_LogicalHeight);

    if (scale <= 0.0f) return false;

    const float offsetX = (viewportWidth - m_LogicalWidth * scale) * 0.5f;
    const float offsetY = (viewportHeight - m_LogicalHeight * scale) * 0.5f;

    result.x = (mouseX - offsetX) / scale;
    result.y = (mouseY - offsetY) / scale;

    return mouseX >= offsetX &&
           mouseY >= offsetY &&
           mouseX <= offsetX + m_LogicalWidth * scale &&
           mouseY <= offsetY + m_LogicalHeight * scale;
}

void UIRenderer::UpdateInput(
    UICanvas& canvas,
    const Input& input,
    float viewportX,
    float viewportY,
    float viewportWidth,
    float viewportHeight,
    float deltaTime)
{
    UIWidget* root = canvas.GetRoot();
    if (!root) return;

    if (m_InputCanvas != &canvas || m_InputCanvasRevision != canvas.GetRevision()) {
        auto reset = [&](auto&& self, UIWidget& node) -> void {
            if (auto* text = dynamic_cast<UITextInput*>(&node)) text->SetFocused(false);
            if (auto* button = dynamic_cast<UIButton*>(&node)) button->SetPressed(false);
            for (const auto& child : node.GetChildren()) self(self, *child);
        };
        reset(reset, *root);
        m_PressedCanvasButton = nullptr; m_FocusedTextInput = nullptr; m_DraggedSlider = nullptr;
        m_InputCanvas = &canvas; m_InputCanvasRevision = canvas.GetRevision();
    }
    // Traverse live objects before accessing any retained focus/capture pointer.
    auto contains = [&](auto&& self, UIWidget& node, const UIWidget* target, std::uint64_t identity) -> bool {
        if (&node == target) return identity == 0 || node.GetInstanceId() == identity;
        for (const auto& child : node.GetChildren()) if (self(self, *child, target, identity)) return true;
        return false;
    };
    auto active = [&](UIWidget* widget, std::uint64_t identity = 0) {
        if (!widget || !contains(contains, *root, widget, identity)) return false;
        for (auto* parent = widget; parent; parent = parent->GetParent())
            if (!parent->IsVisible() || !parent->IsEnabled()) return false;
        return widget->IsHitTestVisible();
    };
    if (!active(m_PressedCanvasButton, m_PressedButtonId)) m_PressedCanvasButton = nullptr;
    if (!active(m_DraggedSlider, m_DraggedSliderId)) m_DraggedSlider = nullptr;
    if (!active(m_FocusedTextInput, m_FocusedTextId)) {
        if (m_FocusedTextInput && contains(contains, *root, m_FocusedTextInput, m_FocusedTextId)) m_FocusedTextInput->SetFocused(false);
        m_FocusedTextInput = nullptr;
    }
    for (const auto& child : root->GetChildren()) if (child) ResetButtonInput(*child);
    if (!m_MouseInteractionEnabled || !root->IsVisible() || !root->IsEnabled()) {
        if (m_PressedCanvasButton) m_PressedCanvasButton->SetPressed(false);
        m_PressedCanvasButton = nullptr; m_DraggedSlider = nullptr;
        if (m_FocusedTextInput) m_FocusedTextInput->SetFocused(false);
        m_FocusedTextInput = nullptr;
        return;
    }

    Vec2 mouse;
    const float localX = input.GetMouseX() - viewportX;
    const float localY = input.GetMouseY() - viewportY;
    const bool inside = ViewportToCanvas(localX, localY, viewportWidth, viewportHeight, mouse);
    const UIRect canvasRect{0.0f, 0.0f, m_LogicalWidth, m_LogicalHeight};

    // Route wheel input to the deepest visible scroll box beneath the cursor.
    // The box clips both drawing and hit testing, so off-screen buttons
    // cannot receive clicks.
    if (inside && std::abs(input.GetMouseWheelY()) > 0.001f)
    {
        std::function<UIScrollBox*(UIWidget&, const UIRect&)> findScroll =
            [&](UIWidget& node, const UIRect& parent) -> UIScrollBox*
        {
            if (!node.IsVisible() || !node.IsEnabled()) return nullptr;
            const UIRect rect = node.GetParent() ? UILayout::Calculate(node, parent) : parent;
            if (auto* scroll = dynamic_cast<UIScrollBox*>(&node))
                if (mouse.x < rect.x || mouse.y < rect.y ||
                    mouse.x >= rect.x+rect.width || mouse.y >= rect.y+rect.height)
                    return nullptr;
            UIRect contentRect = rect;
            if (auto* scroll = dynamic_cast<UIScrollBox*>(&node))
                contentRect.y -= scroll->GetScrollOffset();
            const auto& children = node.GetChildren();
            for (auto it = children.rbegin(); it != children.rend(); ++it)
                if (UIScrollBox* nested = findScroll(**it, contentRect)) return nested;
            if (auto* scroll = dynamic_cast<UIScrollBox*>(&node))
                if (mouse.x >= rect.x && mouse.y >= rect.y &&
                    mouse.x < rect.x+rect.width && mouse.y < rect.y+rect.height)
                {
                    float contentHeight = scroll->GetContentHeight();
                    for (const auto& child : scroll->GetChildren())
                    {
                        if (!child || !child->IsVisible()) continue;
                        const UIRect childBounds = UILayout::Calculate(*child, rect);
                        contentHeight = std::max(contentHeight,
                            childBounds.y+childBounds.height-rect.y);
                    }
                    scroll->SetScrollOffset(std::clamp(
                        scroll->GetScrollOffset() - input.GetMouseWheelY()*55.0f,
                        0.0f, std::max(0.0f, contentHeight-rect.height)));
                    return scroll;
                }
            return nullptr;
        };
        findScroll(*root, canvasRect);
    }

    UIWidget* hit = inside ? FindTopControl(*root, canvasRect, mouse) : nullptr;
    // One topmost control owns input across all interactive widget types.
    if (hit && !active(hit)) hit = nullptr;
    UIButton* hovered = dynamic_cast<UIButton*>(hit);
    UITextInput* hoveredInput = dynamic_cast<UITextInput*>(hit);
    UISlider* hoveredSlider = dynamic_cast<UISlider*>(hit);

    if (hovered) hovered->SetHovered(true);

    if (input.IsMouseButtonPressed(SDL_BUTTON_LEFT))
    {
        m_PressedCanvasButton = hovered;
        m_DraggedSlider = hoveredSlider;
        m_PressedButtonId = hovered ? hovered->GetInstanceId() : 0;
        m_DraggedSliderId = hoveredSlider ? hoveredSlider->GetInstanceId() : 0;
        if (m_FocusedTextInput && m_FocusedTextInput != hoveredInput)
            m_FocusedTextInput->SetFocused(false);
        m_FocusedTextInput = hoveredInput;
        m_FocusedTextId = hoveredInput ? hoveredInput->GetInstanceId() : 0;
        if (m_FocusedTextInput)
            m_FocusedTextInput->SetFocused(true);
    }

    if (m_FocusedTextInput)
    {
        // Match normal text-editor behavior: delete once immediately, then
        // repeat after a short hold delay at a steady rate.
        constexpr float repeatDelay = 0.4f;
        constexpr float repeatInterval = 0.05f;
        const float frameSeconds = std::clamp(deltaTime, 0.0f, 0.1f);

        auto repeatKey = [&](SDL_Scancode key, float& heldTime, float& repeatTime, auto action)
        {
            if (!input.IsKeyDown(key))
            {
                heldTime = 0.0f;
                repeatTime = 0.0f;
                return;
            }

            if (input.IsKeyPressed(key))
            {
                action();
                heldTime = 0.0f;
                repeatTime = 0.0f;
                return;
            }

            heldTime += frameSeconds;
            if (heldTime < repeatDelay)
                return;

            repeatTime += frameSeconds;
            while (repeatTime >= repeatInterval)
            {
                action();
                repeatTime -= repeatInterval;
            }
        };

        const bool ctrl = input.IsKeyDown(SDL_SCANCODE_LCTRL) || input.IsKeyDown(SDL_SCANCODE_RCTRL);

        if (ctrl && input.IsKeyPressed(SDL_SCANCODE_A)) m_FocusedTextInput->SelectAll();
        else if (ctrl && input.IsKeyPressed(SDL_SCANCODE_C) && m_FocusedTextInput->HasSelection())
            SDL_SetClipboardText(m_FocusedTextInput->GetText().c_str());
        else if (ctrl && input.IsKeyPressed(SDL_SCANCODE_X) && m_FocusedTextInput->HasSelection())
        {
            SDL_SetClipboardText(m_FocusedTextInput->GetText().c_str());
            m_FocusedTextInput->SetText("");
        }
        else if (ctrl && input.IsKeyPressed(SDL_SCANCODE_V))
        {
            char* clipboard = SDL_GetClipboardText();
            if (clipboard) { m_FocusedTextInput->Insert(clipboard); SDL_free(clipboard); }
        }
        else
        {
            repeatKey(SDL_SCANCODE_BACKSPACE, m_BackspaceHeldTime, m_BackspaceRepeatTime,
                [&]() { m_FocusedTextInput->Backspace(); });
            repeatKey(SDL_SCANCODE_DELETE, m_DeleteHeldTime, m_DeleteRepeatTime,
                [&]() { m_FocusedTextInput->DeleteForward(); });
            if (input.IsKeyPressed(SDL_SCANCODE_LEFT)) m_FocusedTextInput->MoveCursorLeft();
            if (input.IsKeyPressed(SDL_SCANCODE_RIGHT)) m_FocusedTextInput->MoveCursorRight();
            if (input.IsKeyPressed(SDL_SCANCODE_HOME)) m_FocusedTextInput->SetCursor(0);
            if (input.IsKeyPressed(SDL_SCANCODE_END)) m_FocusedTextInput->SetCursor(m_FocusedTextInput->GetText().size());

            // SDL supplies layout-aware text, including shifted punctuation.
            m_FocusedTextInput->Insert(input.GetTextInput());
        }

        if (input.IsKeyPressed(SDL_SCANCODE_RETURN) || input.IsKeyPressed(SDL_SCANCODE_ESCAPE))
        {
            m_BackspaceHeldTime = m_DeleteHeldTime = 0.0f;
            m_BackspaceRepeatTime = m_DeleteRepeatTime = 0.0f;
            m_FocusedTextInput->SetFocused(false);
            m_FocusedTextInput = nullptr;
        }
    }

    if (m_PressedCanvasButton)
        m_PressedCanvasButton->SetPressed(m_PressedCanvasButton == hovered && input.IsMouseButtonDown(SDL_BUTTON_LEFT));

    if (m_DraggedSlider && input.IsMouseButtonDown(SDL_BUTTON_LEFT)) {
        std::vector<UIWidget*> ancestors;
        for (UIWidget* node = m_DraggedSlider; node && node != root; node = node->GetParent()) ancestors.push_back(node);
        UIRect sliderRect = canvasRect;
        for (auto it = ancestors.rbegin(); it != ancestors.rend(); ++it) sliderRect = UILayout::Calculate(**it, sliderRect);
        if (sliderRect.width > 0.0f) m_DraggedSlider->SetValue((mouse.x-sliderRect.x)/sliderRect.width);
    }

    if (input.IsMouseButtonReleased(SDL_BUTTON_LEFT))
    {
        if (m_PressedCanvasButton && m_PressedCanvasButton == hovered)
        {
            m_PressedCanvasButton->SetClicked(true);
            if (m_Audio && !m_PressedCanvasButton->GetClickSoundPath().empty())
                m_Audio->PlaySound(m_PressedCanvasButton->GetClickSoundPath(), 0.65f * m_Audio->GetUIVolume());
        }
        if (m_PressedCanvasButton)
            m_PressedCanvasButton->SetPressed(false);
        m_PressedCanvasButton = nullptr;
        m_DraggedSlider = nullptr;
    }
}

void UIRenderer::ResetButtonInput(UIWidget& widget)
{
    if (UIButton* button = dynamic_cast<UIButton*>(&widget))
    {
        button->SetHovered(false);
        if (button != m_PressedCanvasButton)
            button->SetPressed(false);
    }
    for (const auto& child : widget.GetChildren())
        if (child) ResetButtonInput(*child);
}

UIWidget* UIRenderer::FindTopControl(UIWidget& widget, const UIRect& parentRect, const Vec2& mouse)
{
    if (!widget.IsVisible()) return nullptr;
    const UIRect rect = widget.GetParent() ? UILayout::Calculate(widget, parentRect) : parentRect;
    UIRect contentRect = rect;
    if (const auto* scroll = dynamic_cast<const UIScrollBox*>(&widget))
    {
        // Scroll boxes are clipping ancestors, not just visual containers.
        if (mouse.x < rect.x || mouse.y < rect.y ||
            mouse.x >= rect.x+rect.width || mouse.y >= rect.y+rect.height)
            return nullptr;
        contentRect.y -= scroll->GetScrollOffset();
    }
    if (widget.IsEnabled()) {
        const auto& children = widget.GetChildren();
        for (auto it = children.rbegin(); it != children.rend(); ++it)
            if (UIWidget* hit = FindTopControl(**it, contentRect, mouse)) return hit;
    }
    const bool interactive = dynamic_cast<UIButton*>(&widget) || dynamic_cast<UITextInput*>(&widget) || dynamic_cast<UISlider*>(&widget);
    if (!interactive || !widget.IsHitTestVisible() || rect.width <= 0 || rect.height <= 0 ||
        mouse.x < rect.x || mouse.y < rect.y || mouse.x >= rect.x+rect.width || mouse.y >= rect.y+rect.height) return nullptr;
    const float radius = std::min(widget.GetCornerRadius(), std::min(rect.width, rect.height)*0.5f);
    const float dx = mouse.x-std::clamp(mouse.x, rect.x+radius, rect.x+rect.width-radius);
    const float dy = mouse.y-std::clamp(mouse.y, rect.y+radius, rect.y+rect.height-radius);
    return dx*dx+dy*dy <= radius*radius ? &widget : nullptr;
}
