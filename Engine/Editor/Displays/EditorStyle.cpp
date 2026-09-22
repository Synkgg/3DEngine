#include "../Editor.h"

void Editor::ApplyEditorStyle()
{
    ImGuiStyle& s = ImGui::GetStyle();

    // Vortek Studio: soft charcoal surfaces, floating controls and a vivid
    // violet/cyan identity. Intentionally distinct from the old UE-like skin.
    s.WindowRounding = 8.0f;
    s.ChildRounding = 7.0f;
    s.FrameRounding = 6.0f;
    s.PopupRounding = 8.0f;
    s.ScrollbarRounding = 9.0f;
    s.GrabRounding = 6.0f;
    s.TabRounding = 6.0f;

    s.WindowBorderSize = 1.0f;
    s.ChildBorderSize = 1.0f;
    s.PopupBorderSize = 1.0f;
    s.FrameBorderSize = 0.0f;
    s.TabBorderSize = 0.0f;

    s.WindowPadding = ImVec2(12.0f, 10.0f);
    s.FramePadding = ImVec2(10.0f, 6.0f);
    s.CellPadding = ImVec2(8.0f, 6.0f);
    s.ItemSpacing = ImVec2(8.0f, 7.0f);
    s.ItemInnerSpacing = ImVec2(6.0f, 5.0f);
    s.IndentSpacing = 20.0f;
    s.ScrollbarSize = 10.0f;
    s.GrabMinSize = 10.0f;
    s.WindowMenuButtonPosition = ImGuiDir_None;

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text]                 = ImVec4(0.91f, 0.92f, 0.96f, 1.00f);
    c[ImGuiCol_TextDisabled]         = ImVec4(0.47f, 0.49f, 0.58f, 1.00f);
    c[ImGuiCol_WindowBg]             = ImVec4(0.035f, 0.038f, 0.055f, 1.00f);
    c[ImGuiCol_ChildBg]              = ImVec4(0.045f, 0.049f, 0.070f, 1.00f);
    c[ImGuiCol_PopupBg]              = ImVec4(0.055f, 0.058f, 0.082f, 0.98f);
    c[ImGuiCol_Border]               = ImVec4(0.14f, 0.15f, 0.21f, 0.85f);
    c[ImGuiCol_BorderShadow]         = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg]              = ImVec4(0.075f, 0.080f, 0.110f, 1.00f);
    c[ImGuiCol_FrameBgHovered]       = ImVec4(0.105f, 0.110f, 0.155f, 1.00f);
    c[ImGuiCol_FrameBgActive]        = ImVec4(0.135f, 0.115f, 0.205f, 1.00f);
    c[ImGuiCol_TitleBg]              = ImVec4(0.030f, 0.032f, 0.047f, 1.00f);
    c[ImGuiCol_TitleBgActive]        = ImVec4(0.050f, 0.050f, 0.078f, 1.00f);
    c[ImGuiCol_TitleBgCollapsed]     = c[ImGuiCol_TitleBg];
    c[ImGuiCol_MenuBarBg]            = ImVec4(0.025f, 0.027f, 0.040f, 1.00f);
    c[ImGuiCol_Button]               = ImVec4(0.080f, 0.085f, 0.120f, 1.00f);
    c[ImGuiCol_ButtonHovered]        = ImVec4(0.155f, 0.125f, 0.245f, 1.00f);
    c[ImGuiCol_ButtonActive]         = ImVec4(0.285f, 0.175f, 0.510f, 1.00f);
    c[ImGuiCol_Header]               = ImVec4(0.095f, 0.095f, 0.140f, 1.00f);
    c[ImGuiCol_HeaderHovered]        = ImVec4(0.150f, 0.125f, 0.225f, 1.00f);
    c[ImGuiCol_HeaderActive]         = ImVec4(0.235f, 0.155f, 0.405f, 1.00f);
    c[ImGuiCol_Tab]                  = ImVec4(0.050f, 0.052f, 0.075f, 1.00f);
    c[ImGuiCol_TabHovered]           = ImVec4(0.165f, 0.130f, 0.260f, 1.00f);
    c[ImGuiCol_TabActive]            = ImVec4(0.115f, 0.095f, 0.175f, 1.00f);
    c[ImGuiCol_TabUnfocused]         = ImVec4(0.040f, 0.042f, 0.060f, 1.00f);
    c[ImGuiCol_TabUnfocusedActive]   = ImVec4(0.075f, 0.070f, 0.105f, 1.00f);
    c[ImGuiCol_CheckMark]            = ImVec4(0.52f, 0.40f, 1.00f, 1.00f);
    c[ImGuiCol_SliderGrab]           = ImVec4(0.42f, 0.34f, 0.88f, 1.00f);
    c[ImGuiCol_SliderGrabActive]     = ImVec4(0.64f, 0.50f, 1.00f, 1.00f);
    c[ImGuiCol_ResizeGrip]           = ImVec4(0.31f, 0.73f, 0.88f, 0.22f);
    c[ImGuiCol_ResizeGripHovered]    = ImVec4(0.31f, 0.73f, 0.88f, 0.65f);
    c[ImGuiCol_ResizeGripActive]     = ImVec4(0.52f, 0.40f, 1.00f, 0.90f);
    c[ImGuiCol_Separator]            = ImVec4(0.13f, 0.14f, 0.19f, 1.00f);
    c[ImGuiCol_SeparatorHovered]     = ImVec4(0.31f, 0.73f, 0.88f, 0.75f);
    c[ImGuiCol_SeparatorActive]      = ImVec4(0.52f, 0.40f, 1.00f, 1.00f);
    c[ImGuiCol_TableHeaderBg]        = ImVec4(0.060f, 0.063f, 0.090f, 1.00f);
    c[ImGuiCol_TableBorderStrong]    = ImVec4(0.13f, 0.14f, 0.19f, 1.00f);
    c[ImGuiCol_TableBorderLight]     = ImVec4(0.09f, 0.10f, 0.14f, 1.00f);
    c[ImGuiCol_DockingPreview]       = ImVec4(0.52f, 0.40f, 1.00f, 0.55f);
    c[ImGuiCol_DockingEmptyBg]       = ImVec4(0.025f, 0.027f, 0.040f, 1.00f);
    c[ImGuiCol_NavHighlight]         = ImVec4(0.52f, 0.40f, 1.00f, 0.80f);
}
