#include "../Editor.h"

void Editor::ApplyEditorStyle()
{
    ImGuiStyle& style = ImGui::GetStyle();

    // Compact, flat, UE5-inspired editor chrome.
    style.WindowRounding = 0.0f;
    style.ChildRounding = 2.0f;
    style.FrameRounding = 2.0f;
    style.PopupRounding = 2.0f;
    style.ScrollbarRounding = 2.0f;
    style.GrabRounding = 2.0f;
    style.TabRounding = 2.0f;

    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.TabBorderSize = 0.0f;

    style.WindowPadding = ImVec2(8.0f, 7.0f);
    style.FramePadding = ImVec2(7.0f, 4.0f);
    style.ItemSpacing = ImVec2(7.0f, 5.0f);
    style.ItemInnerSpacing = ImVec2(5.0f, 4.0f);
    style.ScrollbarSize = 11.0f;
    style.IndentSpacing = 18.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(0.86f, 0.88f, 0.91f, 1.0f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.42f, 0.45f, 0.50f, 1.0f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.030f, 0.034f, 0.040f, 1.0f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.038f, 0.043f, 0.050f, 1.0f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.045f, 0.050f, 0.058f, 0.98f);
    colors[ImGuiCol_Border] = ImVec4(0.105f, 0.115f, 0.130f, 1.0f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_FrameBg] = ImVec4(0.065f, 0.072f, 0.082f, 1.0f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.090f, 0.105f, 0.122f, 1.0f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.105f, 0.125f, 0.150f, 1.0f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.025f, 0.028f, 0.033f, 1.0f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.035f, 0.040f, 0.047f, 1.0f);
    colors[ImGuiCol_TitleBgCollapsed] = colors[ImGuiCol_TitleBg];
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.025f, 0.029f, 0.034f, 1.0f);
    colors[ImGuiCol_Button] = ImVec4(0.070f, 0.078f, 0.090f, 1.0f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.105f, 0.125f, 0.150f, 1.0f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.075f, 0.180f, 0.285f, 1.0f);
    colors[ImGuiCol_Header] = ImVec4(0.065f, 0.075f, 0.088f, 1.0f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.090f, 0.125f, 0.160f, 1.0f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.055f, 0.180f, 0.300f, 1.0f);
    colors[ImGuiCol_Tab] = ImVec4(0.035f, 0.040f, 0.047f, 1.0f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.075f, 0.150f, 0.220f, 1.0f);
    colors[ImGuiCol_TabActive] = ImVec4(0.055f, 0.100f, 0.145f, 1.0f);
    colors[ImGuiCol_TabUnfocused] = colors[ImGuiCol_Tab];
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.050f, 0.060f, 0.072f, 1.0f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.10f, 0.55f, 0.92f, 1.0f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.10f, 0.48f, 0.82f, 1.0f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.15f, 0.62f, 1.0f, 1.0f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.10f, 0.48f, 0.82f, 0.25f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.10f, 0.55f, 0.92f, 0.65f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.15f, 0.62f, 1.0f, 0.90f);
    colors[ImGuiCol_Separator] = ImVec4(0.10f, 0.11f, 0.13f, 1.0f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.10f, 0.45f, 0.75f, 1.0f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.12f, 0.58f, 0.95f, 1.0f);
}
