#include "../Editor.h"

void Editor::ApplyEditorStyle()
{
    ImGuiStyle& s = ImGui::GetStyle();

    // Production editor skin: dense, layered charcoal surfaces with a restrained
    // electric-blue interaction color. The viewport stays visually dominant.
    s.WindowRounding = 4.0f;
    s.ChildRounding = 4.0f;
    s.FrameRounding = 4.0f;
    s.PopupRounding = 5.0f;
    s.ScrollbarRounding = 8.0f;
    s.GrabRounding = 4.0f;
    s.TabRounding = 4.0f;

    s.WindowBorderSize = 1.0f;
    s.ChildBorderSize = 1.0f;
    s.PopupBorderSize = 1.0f;
    s.FrameBorderSize = 1.0f;
    s.TabBorderSize = 0.0f;

    s.WindowPadding = ImVec2(9.0f, 8.0f);
    s.FramePadding = ImVec2(8.0f, 5.0f);
    s.CellPadding = ImVec2(8.0f, 6.0f);
    s.ItemSpacing = ImVec2(7.0f, 6.0f);
    s.ItemInnerSpacing = ImVec2(6.0f, 5.0f);
    s.IndentSpacing = 18.0f;
    s.ScrollbarSize = 11.0f;
    s.GrabMinSize = 10.0f;
    s.WindowMenuButtonPosition = ImGuiDir_None;

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text]                 = ImVec4(0.88f, 0.91f, 0.94f, 1.00f);
    c[ImGuiCol_TextDisabled]         = ImVec4(0.43f, 0.48f, 0.53f, 1.00f);
    c[ImGuiCol_WindowBg]             = ImVec4(0.028f, 0.034f, 0.041f, 1.00f);
    c[ImGuiCol_ChildBg]              = ImVec4(0.033f, 0.040f, 0.048f, 1.00f);
    c[ImGuiCol_PopupBg]              = ImVec4(0.035f, 0.043f, 0.052f, 0.99f);
    c[ImGuiCol_Border]               = ImVec4(0.105f, 0.125f, 0.145f, 1.00f);
    c[ImGuiCol_BorderShadow]         = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    c[ImGuiCol_FrameBg]              = ImVec4(0.047f, 0.057f, 0.068f, 1.00f);
    c[ImGuiCol_FrameBgHovered]       = ImVec4(0.065f, 0.080f, 0.095f, 1.00f);
    c[ImGuiCol_FrameBgActive]        = ImVec4(0.070f, 0.120f, 0.160f, 1.00f);

    c[ImGuiCol_TitleBg]              = ImVec4(0.022f, 0.027f, 0.033f, 1.00f);
    c[ImGuiCol_TitleBgActive]        = ImVec4(0.030f, 0.038f, 0.046f, 1.00f);
    c[ImGuiCol_TitleBgCollapsed]     = c[ImGuiCol_TitleBg];
    c[ImGuiCol_MenuBarBg]            = ImVec4(0.018f, 0.023f, 0.029f, 1.00f);

    c[ImGuiCol_Button]               = ImVec4(0.055f, 0.066f, 0.078f, 1.00f);
    c[ImGuiCol_ButtonHovered]        = ImVec4(0.075f, 0.130f, 0.170f, 1.00f);
    c[ImGuiCol_ButtonActive]         = ImVec4(0.055f, 0.300f, 0.460f, 1.00f);

    c[ImGuiCol_Header]               = ImVec4(0.050f, 0.062f, 0.074f, 1.00f);
    c[ImGuiCol_HeaderHovered]        = ImVec4(0.070f, 0.115f, 0.145f, 1.00f);
    c[ImGuiCol_HeaderActive]         = ImVec4(0.055f, 0.260f, 0.390f, 1.00f);

    c[ImGuiCol_Tab]                  = ImVec4(0.026f, 0.032f, 0.039f, 1.00f);
    c[ImGuiCol_TabHovered]           = ImVec4(0.055f, 0.180f, 0.260f, 1.00f);
    c[ImGuiCol_TabActive]            = ImVec4(0.045f, 0.095f, 0.130f, 1.00f);
    c[ImGuiCol_TabUnfocused]         = ImVec4(0.023f, 0.028f, 0.034f, 1.00f);
    c[ImGuiCol_TabUnfocusedActive]   = ImVec4(0.035f, 0.060f, 0.075f, 1.00f);

    c[ImGuiCol_CheckMark]            = ImVec4(0.18f, 0.65f, 0.92f, 1.00f);
    c[ImGuiCol_SliderGrab]           = ImVec4(0.16f, 0.52f, 0.75f, 1.00f);
    c[ImGuiCol_SliderGrabActive]     = ImVec4(0.24f, 0.72f, 1.00f, 1.00f);

    c[ImGuiCol_Separator]            = ImVec4(0.10f, 0.12f, 0.14f, 1.00f);
    c[ImGuiCol_SeparatorHovered]     = ImVec4(0.12f, 0.46f, 0.66f, 1.00f);
    c[ImGuiCol_SeparatorActive]      = ImVec4(0.18f, 0.65f, 0.92f, 1.00f);

    c[ImGuiCol_ResizeGrip]           = ImVec4(0.18f, 0.65f, 0.92f, 0.16f);
    c[ImGuiCol_ResizeGripHovered]    = ImVec4(0.18f, 0.65f, 0.92f, 0.55f);
    c[ImGuiCol_ResizeGripActive]     = ImVec4(0.18f, 0.65f, 0.92f, 0.90f);

    c[ImGuiCol_TableHeaderBg]        = ImVec4(0.035f, 0.043f, 0.052f, 1.00f);
    c[ImGuiCol_TableBorderStrong]    = ImVec4(0.10f, 0.12f, 0.14f, 1.00f);
    c[ImGuiCol_TableBorderLight]     = ImVec4(0.065f, 0.078f, 0.090f, 1.00f);

    c[ImGuiCol_DockingPreview]       = ImVec4(0.18f, 0.65f, 0.92f, 0.45f);
    c[ImGuiCol_DockingEmptyBg]       = ImVec4(0.018f, 0.023f, 0.029f, 1.00f);
    c[ImGuiCol_NavHighlight]         = ImVec4(0.18f, 0.65f, 0.92f, 0.75f);
}
