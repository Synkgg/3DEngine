#include "Window.h"

#include <SDL3/SDL.h>

#include "../../Core/Logger.h"

Window::Window(
    const std::string& title,
    int width,
    int height)
    : m_Window(nullptr),
    m_Title(title),
    m_Width(width),
    m_Height(height),
    m_Fullscreen(false)
{
}

Window::~Window()
{
    Shutdown();
}

bool Window::Initialize()
{
    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_MAJOR_VERSION,
        4
    );

    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_MINOR_VERSION,
        5
    );

    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_PROFILE_MASK,
        SDL_GL_CONTEXT_PROFILE_CORE
    );

    SDL_GL_SetAttribute(
        SDL_GL_DOUBLEBUFFER,
        1
    );

    SDL_GL_SetAttribute(
        SDL_GL_DEPTH_SIZE,
        24
    );

    m_Window =
        SDL_CreateWindow(
            m_Title.c_str(),
            m_Width,
            m_Height,
            SDL_WINDOW_OPENGL |
            SDL_WINDOW_RESIZABLE
        );

    if (m_Window == nullptr)
    {
        Logger::Error(
            std::string(
                "Failed to create window: "
            ) +
            SDL_GetError()
        );

        return false;
    }

    UpdateSize();

    return true;
}

void Window::Shutdown()
{
    if (m_Window != nullptr)
    {
        SDL_DestroyWindow(
            m_Window
        );

        m_Window = nullptr;
    }
}

SDL_Window* Window::GetNativeWindow() const
{
    return m_Window;
}

int Window::GetWidth() const
{
    return m_Width;
}

int Window::GetHeight() const
{
    return m_Height;
}

bool Window::SetFullscreen(
    bool fullscreen)
{
    if (m_Window == nullptr)
    {
        return false;
    }

    if (!SDL_SetWindowFullscreen(
        m_Window,
        fullscreen
    ))
    {
        Logger::Error(
            std::string(
                "Failed to change fullscreen state: "
            ) +
            SDL_GetError()
        );

        return false;
    }

    m_Fullscreen =
        fullscreen;

    return true;
}

bool Window::ToggleFullscreen()
{
    return SetFullscreen(
        !m_Fullscreen
    );
}

bool Window::IsFullscreen() const
{
    return m_Fullscreen;
}

void Window::UpdateSize()
{
    if (m_Window == nullptr)
    {
        return;
    }

    SDL_GetWindowSize(
        m_Window,
        &m_Width,
        &m_Height
    );
}