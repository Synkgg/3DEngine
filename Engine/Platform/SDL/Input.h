#pragma once

#include <SDL3/SDL.h>
#include <array>

class Input
{
public:
    Input();

    void Update();

    bool IsKeyDown(SDL_Scancode key) const;
    bool IsKeyPressed(SDL_Scancode key) const;

    float GetMouseDeltaX() const;
    float GetMouseDeltaY() const;

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

    bool m_MouseCaptured;
};