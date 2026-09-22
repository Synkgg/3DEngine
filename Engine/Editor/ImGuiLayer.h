#pragma once

#include <SDL3/SDL.h>

class ImFont;
class Window;

class ImGuiLayer
{
public:
    ImGuiLayer();
    ~ImGuiLayer();

    bool Initialize(Window& window, SDL_GLContext context);
    void Shutdown();

    void BeginFrame();
    void EndFrame();

    void ProcessEvent(void* event);

    ImFont* GetIconFont() const;

private:
    bool m_Initialized;
    ImFont* m_IconFont;
};