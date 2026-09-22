#pragma once

#include <string>

struct SDL_Window;

class Window
{
public:
    Window(
        const std::string& title,
        int width,
        int height
    );

    ~Window();

    bool Initialize();
    void Shutdown();

    SDL_Window* GetNativeWindow() const;

    int GetWidth() const;
    int GetHeight() const;

    bool SetFullscreen(bool fullscreen);
    bool ToggleFullscreen();
    bool IsFullscreen() const;

    void UpdateSize();

private:
    SDL_Window* m_Window;

    std::string m_Title;

    int m_Width;
    int m_Height;

    bool m_Fullscreen;
};