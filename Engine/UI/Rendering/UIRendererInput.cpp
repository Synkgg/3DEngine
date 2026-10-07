#include "../UIRenderer.h"
#include "../UIText.h"
#include "../UIImage.h"
#include "../UIButton.h"
#include "../UITextInput.h"
#include "../UISlider.h"
#include "../../Platform/SDL/Input.h"
#include "../../Graphics/Renderer.h"
#include "../../Graphics/Texture2D.h"
#include "../../Audio/AudioEngine.h"
#include <algorithm>
#include <cmath>
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
    float viewportHeight)
{
    UIWidget* root = canvas.GetRoot();
    if (!root) return;

    for (const auto& child : root->GetChildren())
        if (child) ResetButtonInput(*child);

    if (!m_MouseInteractionEnabled)
    {
        m_PressedCanvasButton = nullptr;
        if (m_FocusedTextInput)
            m_FocusedTextInput->SetFocused(false);
        m_FocusedTextInput = nullptr;
        return;
    }

    // Loading/clearing a UI replaces the widget tree. Never keep a raw focus
    // pointer into an old canvas: validate it against the current tree first.
    if (m_FocusedTextInput)
    {
        UIWidget* current = root->Find(m_FocusedTextInput->GetName());
        if (current != m_FocusedTextInput)
            m_FocusedTextInput = nullptr;
    }

    Vec2 mouse;
    const float localX = input.GetMouseX() - viewportX;
    const float localY = input.GetMouseY() - viewportY;
    const bool inside = ViewportToCanvas(localX, localY, viewportWidth, viewportHeight, mouse);
    const UIRect canvasRect{0.0f, 0.0f, m_LogicalWidth, m_LogicalHeight};

    UIButton* hovered = nullptr;
    UITextInput* hoveredInput = nullptr;
    UISlider* hoveredSlider = nullptr;
    if (inside)
    {
        // Children are z-sorted ascending, so later hits replace earlier ones
        // and the visually topmost button owns the interaction.
        for (const auto& child : root->GetChildren())
            if (child)
            {
                if (UIButton* hit = FindTopButton(*child, canvasRect, mouse))
                    hovered = hit;
                if (UITextInput* hit = FindTopTextInput(*child, canvasRect, mouse))
                    hoveredInput = hit;
                if (UISlider* hit = FindTopSlider(*child, canvasRect, mouse)) hoveredSlider = hit;
            }
    }

    if (hovered) hovered->SetHovered(true);

    if (inside && input.IsMouseButtonPressed(SDL_BUTTON_LEFT))
    {
        m_PressedCanvasButton = hovered;
        m_DraggedSlider = hoveredSlider;
        if (m_FocusedTextInput && m_FocusedTextInput != hoveredInput)
            m_FocusedTextInput->SetFocused(false);
        m_FocusedTextInput = hoveredInput;
        if (m_FocusedTextInput)
            m_FocusedTextInput->SetFocused(true);
    }

    if (m_FocusedTextInput)
    {
        // Match normal text-editor behavior: delete once immediately, then
        // repeat after a short hold delay at a steady rate.
        constexpr float repeatDelay = 1.f;
        constexpr float repeatInterval = 1.f;
        const float frameSeconds = 1.0f / 60.0f;

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
        const bool shift = input.IsKeyDown(SDL_SCANCODE_LSHIFT) || input.IsKeyDown(SDL_SCANCODE_RSHIFT);

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

            const bool caps = (SDL_GetModState() & SDL_KMOD_CAPS) != 0;
            for (int i = 0; i < 26; ++i)
                if (input.IsKeyPressed(static_cast<SDL_Scancode>(SDL_SCANCODE_A + i)))
                {
                    char ch = static_cast<char>('a' + i);
                    if (shift != caps) ch = static_cast<char>('A' + i);
                    m_FocusedTextInput->Insert(std::string(1, ch));
                }
            // SDL digit scancodes are not laid out numerically: 1..9 are
            // contiguous and 0 comes after 9.
            for (int i = 1; i <= 9; ++i)
            {
                const SDL_Scancode key = static_cast<SDL_Scancode>(SDL_SCANCODE_1 + (i - 1));
                if (input.IsKeyPressed(key))
                    m_FocusedTextInput->Insert(std::string(1, static_cast<char>('0' + i)));
            }
            if (input.IsKeyPressed(SDL_SCANCODE_0))
                m_FocusedTextInput->Insert("0");
            if (input.IsKeyPressed(SDL_SCANCODE_SPACE)) m_FocusedTextInput->Insert(" ");
            if (input.IsKeyPressed(SDL_SCANCODE_PERIOD)) m_FocusedTextInput->Insert(".");
            if (input.IsKeyPressed(SDL_SCANCODE_MINUS)) m_FocusedTextInput->Insert(shift ? "_" : "-");
            if (input.IsKeyPressed(SDL_SCANCODE_SLASH)) m_FocusedTextInput->Insert("/");
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
        m_PressedCanvasButton->SetPressed(true);

    if (m_DraggedSlider && inside && input.IsMouseButtonDown(SDL_BUTTON_LEFT))
    {
        UIRect sliderRect=canvasRect;
        if(m_DraggedSlider->GetParent() && m_DraggedSlider->GetParent()!=root) sliderRect=UILayout::Calculate(*m_DraggedSlider->GetParent(),canvasRect);
        sliderRect=UILayout::Calculate(*m_DraggedSlider,sliderRect);
        if(sliderRect.width>0.0f) m_DraggedSlider->SetValue((mouse.x-sliderRect.x)/sliderRect.width);
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

UIButton* UIRenderer::FindTopButton(
    UIWidget& widget,
    const UIRect& parentRect,
    const Vec2& mouse)
{
    if (!widget.IsVisible() || !widget.IsEnabled())
        return nullptr;

    const UIRect rect = UILayout::Calculate(widget, parentRect);
    UIButton* result = nullptr;

    for (const auto& child : widget.GetChildren())
        if (child)
            if (UIButton* hit = FindTopButton(*child, rect, mouse))
                result = hit;

    const bool hit = widget.IsHitTestVisible() &&
        mouse.x >= rect.x && mouse.x <= rect.x + rect.width &&
        mouse.y >= rect.y && mouse.y <= rect.y + rect.height;

    if (hit)
        if (UIButton* button = dynamic_cast<UIButton*>(&widget))
            result = button;

    return result;
}

UISlider* UIRenderer::FindTopSlider(UIWidget& widget,const UIRect& parentRect,const Vec2& mouse)
{
    if(!widget.IsVisible()||!widget.IsEnabled())return nullptr;
    const UIRect rect=UILayout::Calculate(widget,parentRect); UISlider* result=nullptr;
    for(const auto& child:widget.GetChildren())if(child)if(UISlider* hit=FindTopSlider(*child,rect,mouse))result=hit;
    const bool hit=widget.IsHitTestVisible()&&mouse.x>=rect.x&&mouse.x<=rect.x+rect.width&&mouse.y>=rect.y&&mouse.y<=rect.y+rect.height;
    if(hit)if(UISlider* slider=dynamic_cast<UISlider*>(&widget))result=slider; return result;
}

UITextInput* UIRenderer::FindTopTextInput(
    UIWidget& widget, const UIRect& parentRect, const Vec2& mouse)
{
    if (!widget.IsVisible() || !widget.IsEnabled()) return nullptr;
    const UIRect rect = UILayout::Calculate(widget, parentRect);
    UITextInput* result = nullptr;
    for (const auto& child : widget.GetChildren())
        if (child)
            if (UITextInput* hit = FindTopTextInput(*child, rect, mouse))
                result = hit;
    const bool hit = widget.IsHitTestVisible() &&
        mouse.x >= rect.x && mouse.x <= rect.x + rect.width &&
        mouse.y >= rect.y && mouse.y <= rect.y + rect.height;
    if (hit)
        if (UITextInput* input = dynamic_cast<UITextInput*>(&widget))
            result = input;
    return result;
}
