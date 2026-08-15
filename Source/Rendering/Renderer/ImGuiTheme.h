#pragma once

#include <imgui.h>

namespace ImGuiTheme
{
inline constexpr ImVec4 Selected = {0.55f, 0.07f, 0.09f, 1.0f};
inline constexpr ImVec4 SelectedBorder = {0.78f, 0.12f, 0.15f, 1.0f};
inline constexpr ImU32 SelectedOutline = IM_COL32(199, 31, 38, 255);
inline constexpr ImU32 SelectedAccent = IM_COL32(214, 42, 50, 255);

inline constexpr ImVec4 RedButton = {0.58f, 0.06f, 0.08f, 1.0f};
inline constexpr ImVec4 RedButtonHovered = {0.76f, 0.10f, 0.13f, 1.0f};
inline constexpr ImVec4 RedButtonActive = {0.90f, 0.15f, 0.19f, 1.0f};

inline constexpr ImVec4 BlueSelected = {0.02f, 0.25f, 0.62f, 1.0f};
inline constexpr ImVec4 BlueSelectedBorder = {0.05f, 0.48f, 0.95f, 1.0f};
inline constexpr ImVec4 BlueButton = {0.02f, 0.20f, 0.50f, 0.90f};
inline constexpr ImVec4 BlueButtonHovered = {0.03f, 0.36f, 0.78f, 0.95f};
inline constexpr ImVec4 BlueButtonActive = {0.04f, 0.47f, 0.96f, 1.0f};

inline constexpr ImVec4 YellowButton = {0.68f, 0.46f, 0.02f, 1.0f};
inline constexpr ImVec4 YellowButtonHovered = {0.84f, 0.61f, 0.04f, 1.0f};
inline constexpr ImVec4 YellowButtonActive = {0.96f, 0.74f, 0.10f, 1.0f};
inline constexpr ImVec4 YellowSelected = {0.58f, 0.38f, 0.02f, 1.0f};
inline constexpr ImVec4 YellowSelectedBorder = {0.90f, 0.65f, 0.06f, 1.0f};

inline constexpr ImVec4 StartVstgButton = {0.04f, 0.36f, 0.70f, 1.0f};
inline constexpr ImVec4 StartVstgButtonHovered = {0.06f, 0.50f, 0.90f, 1.0f};
inline constexpr ImVec4 StartVstgButtonActive = {0.10f, 0.60f, 1.0f, 1.0f};

inline void ApplyRedTheme(ImGuiStyle& style)
{
	style.Colors[ImGuiCol_FrameBgHovered] = {0.42f, 0.04f, 0.06f, 0.85f};
	style.Colors[ImGuiCol_FrameBgActive] = {0.58f, 0.06f, 0.08f, 0.95f};
	style.Colors[ImGuiCol_TitleBgActive] = {0.34f, 0.03f, 0.04f, 0.95f};
	style.Colors[ImGuiCol_CheckMark] = RedButtonActive;
	style.Colors[ImGuiCol_SliderGrab] = RedButton;
	style.Colors[ImGuiCol_SliderGrabActive] = RedButtonActive;
	style.Colors[ImGuiCol_Button] = {0.38f, 0.04f, 0.05f, 0.82f};
	style.Colors[ImGuiCol_ButtonHovered] = RedButtonHovered;
	style.Colors[ImGuiCol_ButtonActive] = RedButtonActive;
	style.Colors[ImGuiCol_Header] = {0.46f, 0.05f, 0.06f, 0.88f};
	style.Colors[ImGuiCol_HeaderHovered] = RedButtonHovered;
	style.Colors[ImGuiCol_HeaderActive] = RedButtonActive;
	style.Colors[ImGuiCol_SeparatorHovered] = RedButtonHovered;
	style.Colors[ImGuiCol_SeparatorActive] = RedButtonActive;
	style.Colors[ImGuiCol_ResizeGrip] = {0.46f, 0.05f, 0.06f, 0.50f};
	style.Colors[ImGuiCol_ResizeGripHovered] = {0.76f, 0.10f, 0.13f, 0.80f};
	style.Colors[ImGuiCol_ResizeGripActive] = RedButtonActive;
	style.Colors[ImGuiCol_Tab] = {0.26f, 0.03f, 0.04f, 0.90f};
	style.Colors[ImGuiCol_TabHovered] = Selected;
	style.Colors[ImGuiCol_TabSelected] = Selected;
	style.Colors[ImGuiCol_TabSelectedOverline] = SelectedBorder;
	style.Colors[ImGuiCol_TabDimmed] = {0.18f, 0.025f, 0.03f, 0.90f};
	style.Colors[ImGuiCol_TabDimmedSelected] = Selected;
	style.Colors[ImGuiCol_TabDimmedSelectedOverline] = SelectedBorder;
	style.Colors[ImGuiCol_TextLink] = SelectedBorder;
	style.Colors[ImGuiCol_TextSelectedBg] = {0.58f, 0.06f, 0.08f, 0.55f};
	style.Colors[ImGuiCol_DockingPreview] = {0.90f, 0.15f, 0.19f, 0.70f};
	style.Colors[ImGuiCol_NavCursor] = SelectedBorder;
}

inline void ApplyYellowTheme(ImGuiStyle& style)
{
	style.Colors[ImGuiCol_FrameBgHovered] = {0.42f, 0.27f, 0.02f, 0.85f};
	style.Colors[ImGuiCol_FrameBgActive] = {0.58f, 0.38f, 0.02f, 0.95f};
	style.Colors[ImGuiCol_TitleBgActive] = {0.34f, 0.22f, 0.01f, 0.95f};
	style.Colors[ImGuiCol_CheckMark] = YellowButtonActive;
	style.Colors[ImGuiCol_SliderGrab] = YellowButton;
	style.Colors[ImGuiCol_SliderGrabActive] = YellowButtonActive;
	style.Colors[ImGuiCol_Button] = {0.38f, 0.25f, 0.01f, 0.82f};
	style.Colors[ImGuiCol_ButtonHovered] = YellowButtonHovered;
	style.Colors[ImGuiCol_ButtonActive] = YellowButtonActive;
	style.Colors[ImGuiCol_Header] = {0.46f, 0.30f, 0.02f, 0.88f};
	style.Colors[ImGuiCol_HeaderHovered] = YellowButtonHovered;
	style.Colors[ImGuiCol_HeaderActive] = YellowButtonActive;
	style.Colors[ImGuiCol_SeparatorHovered] = YellowButtonHovered;
	style.Colors[ImGuiCol_SeparatorActive] = YellowButtonActive;
	style.Colors[ImGuiCol_ResizeGrip] = {0.46f, 0.30f, 0.02f, 0.50f};
	style.Colors[ImGuiCol_ResizeGripHovered] = {0.84f, 0.61f, 0.04f, 0.80f};
	style.Colors[ImGuiCol_ResizeGripActive] = YellowButtonActive;
	style.Colors[ImGuiCol_Tab] = {0.26f, 0.17f, 0.01f, 0.90f};
	style.Colors[ImGuiCol_TabHovered] = YellowSelected;
	style.Colors[ImGuiCol_TabSelected] = YellowSelected;
	style.Colors[ImGuiCol_TabSelectedOverline] = YellowSelectedBorder;
	style.Colors[ImGuiCol_TabDimmed] = {0.18f, 0.12f, 0.01f, 0.90f};
	style.Colors[ImGuiCol_TabDimmedSelected] = YellowSelected;
	style.Colors[ImGuiCol_TabDimmedSelectedOverline] = YellowSelectedBorder;
	style.Colors[ImGuiCol_TextLink] = YellowSelectedBorder;
	style.Colors[ImGuiCol_TextSelectedBg] = {0.58f, 0.38f, 0.02f, 0.55f};
	style.Colors[ImGuiCol_DockingPreview] = {0.96f, 0.74f, 0.10f, 0.70f};
	style.Colors[ImGuiCol_NavCursor] = YellowSelectedBorder;
}
} // namespace ImGuiTheme
