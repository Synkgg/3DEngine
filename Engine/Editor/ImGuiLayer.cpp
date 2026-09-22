#include "ImGuiLayer.h"

#include "../Platform/SDL/Window.h"

#include "Fonts/FontAwesome.h"
#include "Fonts/IconsFontAwesome6.h"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>
#include <ImGuizmo.h>

ImGuiLayer::ImGuiLayer()
    : m_Initialized(false),
    m_IconFont(nullptr)
{
}

ImGuiLayer::~ImGuiLayer()
{
    Shutdown();
}

bool ImGuiLayer::Initialize(Window& window, SDL_GLContext context)
{
    IMGUI_CHECKVERSION();

    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();

    io.Fonts->AddFontDefault();

    ImFontConfig fontConfig;
    fontConfig.FontDataOwnedByAtlas = false;

    static const ImWchar iconRanges[] = {
        ICON_MIN_FA,
        ICON_MAX_FA,
        0
    };

    m_IconFont = io.Fonts->AddFontFromMemoryTTF(
        const_cast<unsigned char*>(g_FontAwesomeData),
        static_cast<int>(g_FontAwesomeDataSize),
        32.0f,
        &fontConfig,
        iconRanges
    );

    if (!ImGui_ImplSDL3_InitForOpenGL(
        window.GetNativeWindow(),
        context))
    {
        return false;
    }

    if (!ImGui_ImplOpenGL3_Init("#version 450"))
    {
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    m_Initialized = true;

    return true;
}

void ImGuiLayer::Shutdown()
{
    if (!m_Initialized)
    {
        return;
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();

    ImGui::DestroyContext();

    m_Initialized = false;
}

void ImGuiLayer::BeginFrame()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    ImGuizmo::BeginFrame();

    ImGui::DockSpaceOverViewport();
}
void ImGuiLayer::EndFrame()
{
    ImGui::Render();

    ImGui_ImplOpenGL3_RenderDrawData(
        ImGui::GetDrawData()
    );
}

void ImGuiLayer::ProcessEvent(void* event)
{
    ImGui_ImplSDL3_ProcessEvent(
        static_cast<SDL_Event*>(event)
    );
}

ImFont* ImGuiLayer::GetIconFont() const
{
    return m_IconFont;
}