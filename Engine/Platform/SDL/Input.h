#pragma once

#include <SDL3/SDL.h>
#include <array>
#include <string>

class Input
{
public:
    Input();

    void Update();
    void UpdateMouseState(float x, float y, SDL_MouseButtonFlags buttons);
    void ProcessEvent(const SDL_Event& event);
    const std::string& GetTextInput() const { return m_TextInput; }

    bool IsKeyDown(SDL_Scancode key) const;
    bool IsKeyPressed(SDL_Scancode key) const;

    float GetMouseDeltaX() const;
    float GetMouseDeltaY() const;
    float GetMouseWheelY() const { return m_MouseWheelY; }

    bool IsMouseButtonDown(Uint8 button) const;
    bool IsMouseButtonPressed(Uint8 button) const;
    bool IsMouseButtonReleased(Uint8 button) const;
    float GetMouseX() const;
    float GetMouseY() const;

    void SetMouseCapture(SDL_Window* window, bool captured);

private:
    const bool* m_KeyboardState;

    std::array<bool, SDL_SCANCODE_COUNT>
        m_CurrentKeyboardState;

    std::array<bool, SDL_SCANCODE_COUNT>
        m_PreviousKeyboardState;

    float m_MouseDeltaX;
    float m_MouseDeltaY;
    SDL_MouseButtonFlags m_MouseButtons;
    SDL_MouseButtonFlags m_PreviousMouseButtons;
    float m_MouseX;
    float m_MouseY;
    float m_MouseWheelY = 0.0f;
    float m_PendingMouseWheelY = 0.0f;

    bool m_MouseCaptured;
    std::string m_PendingTextInput, m_TextInput;
};
