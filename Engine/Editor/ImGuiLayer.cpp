#include "ImGuiLayer.h"

#include "../Platform/SDL/Window.h"
#include "../Core/Logger.h"

#include "Fonts/FontAwesome.h"
#include "Fonts/IconsFontAwesome6.h"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
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

bool ImGuiLayer::Initialize(Window& window)
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

    // SDL owns input/window integration only. GPU rendering is now owned by
    // the Vulkan/NRI renderer, so no OpenGL renderer backend is initialized.
    if (!ImGui_ImplSDL3_InitForVulkan(window.GetNativeWindow()))
    {
        Logger::Error("ImGui SDL3 Vulkan platform backend initialization failed.");
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

    ImGui_ImplSDL3_Shutdown();

    ImGui::DestroyContext();

    m_Initialized = false;
}

void ImGuiLayer::BeginFrame()
{
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    ImGuizmo::BeginFrame();
}
void ImGuiLayer::EndFrame()
{
    // Build draw data here. The NRI renderer consumes ImGui::GetDrawData()
    // while recording the Vulkan command buffer.
    ImGui::Render();
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