#include "ImGuiLayer.h"

#include "../Platform/SDL/Window.h"
#include "../Core/Logger.h"

#include "Fonts/FontAwesome.h"
#include "Fonts/IconsFontAwesome6.h"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>
#include <ImGuizmo.h>
#include <cstdio>

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

    // Inter is bundled with the engine and loaded into the ImGui atlas.
    // It gives the editor the smooth modern UI typography used by the
    // runtime canvas instead of ImGui's pixel-oriented default font.
    ImFontConfig editorFontConfig{};
    editorFontConfig.OversampleH = 3;
    editorFontConfig.OversampleV = 2;
    editorFontConfig.PixelSnapH = false;

    ImFont* editorFont = nullptr;

    // ImGui asserts in Debug builds when AddFontFromFileTTF cannot open the
    // path. Check it ourselves first so launching from out/build also works.
    if (FILE* fontFile = std::fopen(
        "Engine/Editor/Fonts/InterVariable.ttf", "rb"))
    {
        std::fclose(fontFile);
        editorFont = io.Fonts->AddFontFromFileTTF(
            "Engine/Editor/Fonts/InterVariable.ttf",
            17.0f,
            &editorFontConfig,
            io.Fonts->GetGlyphRangesDefault()
        );
    }

    if (editorFont == nullptr)
        editorFont = io.Fonts->AddFontDefault();

    io.FontDefault = editorFont;

    ImFontConfig fontConfig{};
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

    // OpenGL3 must be initialized before SDL3. The SDL3 backend queries
    // platform-interface state that expects the renderer backend data to
    // already exist with this ImGui version.
    if (!ImGui_ImplOpenGL3_Init("#version 450"))
    {
        Logger::Error("ImGui OpenGL3 backend initialization failed.");
        ImGui::DestroyContext();
        return false;
    }

    if (!ImGui_ImplSDL3_InitForOpenGL(
        window.GetNativeWindow(),
        context))
    {
        Logger::Error("ImGui SDL3 backend initialization failed.");
        ImGui_ImplOpenGL3_Shutdown();
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