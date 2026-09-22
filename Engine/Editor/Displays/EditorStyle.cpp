#include "../Editor.h"

void Editor::ApplyEditorStyle()
{
	ImGuiStyle& style =
		ImGui::GetStyle();

	style.WindowRounding = 5.0f;
	style.ChildRounding = 4.0f;
	style.FrameRounding = 4.0f;
	style.PopupRounding = 4.0f;
	style.ScrollbarRounding = 4.0f;
	style.GrabRounding = 4.0f;

	style.WindowBorderSize = 1.0f;
	style.ChildBorderSize = 1.0f;
	style.PopupBorderSize = 1.0f;
	style.FrameBorderSize = 0.0f;

	style.WindowPadding =
		ImVec2(8.0f, 8.0f);

	style.FramePadding =
		ImVec2(7.0f, 5.0f);

	style.ItemSpacing =
		ImVec2(7.0f, 5.0f);

	style.ItemInnerSpacing =
		ImVec2(5.0f, 4.0f);

	style.ScrollbarSize = 12.0f;

	ImVec4* colors =
		style.Colors;

	colors[ImGuiCol_WindowBg] =
		ImVec4(
			0.055f,
			0.065f,
			0.085f,
			1.0f
		);

	colors[ImGuiCol_ChildBg] =
		ImVec4(
			0.045f,
			0.052f,
			0.068f,
			1.0f
		);

	colors[ImGuiCol_PopupBg] =
		ImVec4(
			0.07f,
			0.08f,
			0.10f,
			1.0f
		);

	colors[ImGuiCol_Border] =
		ImVec4(
			0.16f,
			0.18f,
			0.22f,
			1.0f
		);

	colors[ImGuiCol_FrameBg] =
		ImVec4(
			0.09f,
			0.105f,
			0.13f,
			1.0f
		);

	colors[ImGuiCol_FrameBgHovered] =
		ImVec4(
			0.12f,
			0.15f,
			0.19f,
			1.0f
		);

	colors[ImGuiCol_FrameBgActive] =
		ImVec4(
			0.14f,
			0.17f,
			0.22f,
			1.0f
		);

	colors[ImGuiCol_TitleBg] =
		ImVec4(
			0.055f,
			0.065f,
			0.085f,
			1.0f
		);

	colors[ImGuiCol_TitleBgActive] =
		ImVec4(
			0.07f,
			0.085f,
			0.11f,
			1.0f
		);

	colors[ImGuiCol_MenuBarBg] =
		ImVec4(
			0.045f,
			0.052f,
			0.068f,
			1.0f
		);

	colors[ImGuiCol_Button] =
		ImVec4(
			0.10f,
			0.12f,
			0.15f,
			1.0f
		);

	colors[ImGuiCol_ButtonHovered] =
		ImVec4(
			0.16f,
			0.20f,
			0.27f,
			1.0f
		);

	colors[ImGuiCol_ButtonActive] =
		ImVec4(
			0.12f,
			0.16f,
			0.22f,
			1.0f
		);

	colors[ImGuiCol_Header] =
		ImVec4(
			0.10f,
			0.13f,
			0.17f,
			1.0f
		);

	colors[ImGuiCol_HeaderHovered] =
		ImVec4(
			0.14f,
			0.18f,
			0.24f,
			1.0f
		);

	colors[ImGuiCol_HeaderActive] =
		ImVec4(
			0.18f,
			0.23f,
			0.30f,
			1.0f
		);

	colors[ImGuiCol_Tab] =
		ImVec4(
			0.07f,
			0.085f,
			0.11f,
			1.0f
		);

	colors[ImGuiCol_TabHovered] =
		ImVec4(
			0.14f,
			0.18f,
			0.24f,
			1.0f
		);

	colors[ImGuiCol_TabActive] =
		ImVec4(
			0.10f,
			0.14f,
			0.19f,
			1.0f
		);

	colors[ImGuiCol_CheckMark] =
		ImVec4(
			0.30f,
			0.60f,
			1.0f,
			1.0f
		);

	colors[ImGuiCol_SliderGrab] =
		ImVec4(
			0.30f,
			0.60f,
			1.0f,
			1.0f
		);

	colors[ImGuiCol_SliderGrabActive] =
		ImVec4(
			0.40f,
			0.68f,
			1.0f,
			1.0f
		);

	colors[ImGuiCol_Text] =
		ImVec4(
			0.88f,
			0.90f,
			0.94f,
			1.0f
		);

	colors[ImGuiCol_TextDisabled] =
		ImVec4(
			0.48f,
			0.52f,
			0.60f,
			1.0f
		);
}