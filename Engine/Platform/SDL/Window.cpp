#include "Window.h"

#include <SDL3/SDL.h>

#include "../../Core/Logger.h"

#if defined(_WIN32)
#include <windows.h>
#include "../../Branding/Resource.h"
#endif

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
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

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

#if defined(_WIN32)
    // Use the same embedded monochrome Velcryn mark for the SDL title bar and
    // taskbar window. Explorer also reads this resource directly from the EXE.
    const SDL_PropertiesID properties = SDL_GetWindowProperties(m_Window);
    HWND nativeWindow = static_cast<HWND>(
        SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr)
    );

    if (nativeWindow != nullptr)
    {
        HINSTANCE instance = GetModuleHandleW(nullptr);
        HICON largeIcon = static_cast<HICON>(
            LoadImageW(instance, MAKEINTRESOURCEW(IDI_VELCRYN_APP), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR)
        );
        HICON smallIcon = static_cast<HICON>(
            LoadImageW(instance, MAKEINTRESOURCEW(IDI_VELCRYN_APP), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR)
        );

        SendMessageW(nativeWindow, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(largeIcon));
        SendMessageW(nativeWindow, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(smallIcon));
    }
#endif

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