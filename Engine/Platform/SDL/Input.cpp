#include "Input.h"

Input::Input()
    : m_KeyboardState(nullptr),
    m_CurrentKeyboardState{},
    m_PreviousKeyboardState{},
    m_MouseDeltaX(0.0f),
    m_MouseDeltaY(0.0f),
    m_MouseButtons(0),
    m_PreviousMouseButtons(0),
    m_MouseX(0.0f),
    m_MouseY(0.0f),
    m_MouseCaptured(false)
{
}

void Input::Update()
{
    m_MouseWheelY = m_PendingMouseWheelY;
    m_PendingMouseWheelY = 0.0f;
    m_TextInput.swap(m_PendingTextInput);
    m_PendingTextInput.clear();
    m_PreviousKeyboardState =
        m_CurrentKeyboardState;

    m_KeyboardState =
        SDL_GetKeyboardState(nullptr);

    if (m_KeyboardState != nullptr)
    {
        for (int i = 0;
            i < SDL_SCANCODE_COUNT;
            ++i)
        {
            m_CurrentKeyboardState[i] =
                m_KeyboardState[i];
        }
    }

    float x, y;
    const auto buttons = SDL_GetMouseState(&x, &y);
    UpdateMouseState(x, y, buttons);

    if (m_MouseCaptured)
    {
        SDL_GetRelativeMouseState(
            &m_MouseDeltaX,
            &m_MouseDeltaY
        );
    }
    else
    {
        m_MouseDeltaX = 0.0f;
        m_MouseDeltaY = 0.0f;
    }
}

bool Input::IsKeyDown(SDL_Scancode key) const
{
    return m_CurrentKeyboardState[key];
}

bool Input::IsKeyPressed(SDL_Scancode key) const
{
    return m_CurrentKeyboardState[key] &&
        !m_PreviousKeyboardState[key];
}

float Input::GetMouseDeltaX() const
{
    return m_MouseDeltaX;
}

float Input::GetMouseDeltaY() const
{
    return m_MouseDeltaY;
}

bool Input::IsMouseButtonDown(Uint8 button) const
{
    return (m_MouseButtons & SDL_BUTTON_MASK(button)) != 0;
}

bool Input::IsMouseButtonPressed(Uint8 button) const
{
    const SDL_MouseButtonFlags mask = SDL_BUTTON_MASK(button);
    return (m_MouseButtons & mask) != 0 && (m_PreviousMouseButtons & mask) == 0;
}

bool Input::IsMouseButtonReleased(Uint8 button) const
{
    const SDL_MouseButtonFlags mask = SDL_BUTTON_MASK(button);
    return (m_MouseButtons & mask) == 0 && (m_PreviousMouseButtons & mask) != 0;
}

float Input::GetMouseX() const { return m_MouseX; }
float Input::GetMouseY() const { return m_MouseY; }

void Input::SetMouseCapture(SDL_Window* window, bool captured)
{
    if (m_MouseCaptured == captured)
    {
        return;
    }

    SDL_SetWindowRelativeMouseMode(window, captured);

    m_MouseDeltaX = 0.0f;
    m_MouseDeltaY = 0.0f;

    if (captured)
    {
        float deltaX = 0.0f;
        float deltaY = 0.0f;

        SDL_GetRelativeMouseState(
            &deltaX,
            &deltaY
        );
    }

    m_MouseCaptured = captured;
}

void Input::ProcessEvent(const SDL_Event& event)
{
    if (event.type == SDL_EVENT_MOUSE_WHEEL)
        m_PendingMouseWheelY += event.wheel.y;
    if (event.type == SDL_EVENT_TEXT_INPUT && event.text.text)
        m_PendingTextInput += event.text.text;
}

void Input::UpdateMouseState(float x, float y, SDL_MouseButtonFlags buttons)
{
    m_PreviousMouseButtons = m_MouseButtons;
    m_MouseButtons = buttons;
    m_MouseX = x; m_MouseY = y;
}
