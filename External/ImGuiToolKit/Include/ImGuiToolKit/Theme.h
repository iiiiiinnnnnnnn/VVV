#pragma once

#include <imgui.h>

namespace ImGuiToolKit::Theme
{
// 配色を変えるときはこの 3 色を調整する。値は SRGBCONV を通す前の sRGB
inline constexpr ImVec4 BackgroundColor = {0.24f, 0.24f, 0.24f, 1.0f};
inline constexpr ImVec4 AccentColor = {0.66f, 0.18f, 0.22f, 1.0f};
inline constexpr ImVec4 TextColor = {0.94f, 0.94f, 0.95f, 1.0f};

inline constexpr ImVec4 White = {1.0f, 1.0f, 1.0f, 1.0f};
inline constexpr ImVec4 Black = {0.0f, 0.0f, 0.0f, 1.0f};

constexpr ImVec4 Blend(const ImVec4& from, const ImVec4& to, float rate)
{
	return {from.x + (to.x - from.x) * rate, from.y + (to.y - from.y) * rate,
		from.z + (to.z - from.z) * rate, from.w + (to.w - from.w) * rate};
}

constexpr ImVec4 Alpha(const ImVec4& color, float alpha)
{
	return {color.x, color.y, color.z, alpha};
}

constexpr ImU32 Pack(const ImVec4& color)
{
	return (static_cast<ImU32>(color.x * 255.0f + 0.5f) << IM_COL32_R_SHIFT) |
		(static_cast<ImU32>(color.y * 255.0f + 0.5f) << IM_COL32_G_SHIFT) |
		(static_cast<ImU32>(color.z * 255.0f + 0.5f) << IM_COL32_B_SHIFT) |
		(static_cast<ImU32>(color.w * 255.0f + 0.5f) << IM_COL32_A_SHIFT);
}

inline constexpr ImVec4 Recessed = Blend(BackgroundColor, Black, 0.15f);
inline constexpr ImVec4 Surface = Blend(BackgroundColor, White, 0.055f);
inline constexpr ImVec4 Raised = Blend(BackgroundColor, White, 0.10f);
inline constexpr ImVec4 Border = Blend(BackgroundColor, TextColor, 0.18f);
inline constexpr ImVec4 AccentHighlight = Blend(AccentColor, White, 0.22f);

// 通常・Hover・Active の順にアクセントの割合を増やし、色相を揃えたまま操作状態を区別する
inline constexpr ImVec4 YellowButton = Blend(Surface, AccentColor, 0.35f);
inline constexpr ImVec4 YellowButtonHovered = Blend(Surface, AccentColor, 0.65f);
inline constexpr ImVec4 YellowButtonActive = AccentColor;
inline constexpr ImVec4 Selected = Blend(BackgroundColor, AccentColor, 0.68f);
inline constexpr ImVec4 SelectedBorder = AccentHighlight;
inline constexpr ImU32 SelectedOutline = Pack(SelectedBorder);
inline constexpr ImU32 SelectedAccent = Pack(AccentColor);

inline void ApplyYellowTheme(ImGuiStyle& style)
{
	style.Alpha = 1.0f;
	style.WindowRounding = 8.0f;
	style.FrameRounding = 4.0f;

	style.Colors[ImGuiCol_Text] = TextColor;
	style.Colors[ImGuiCol_TextDisabled] = Blend(TextColor, BackgroundColor, 0.45f);
	style.Colors[ImGuiCol_WindowBg] = BackgroundColor;
	style.Colors[ImGuiCol_ChildBg] = Blend(BackgroundColor, Black, 0.07f);
	style.Colors[ImGuiCol_PopupBg] = Surface;
	style.Colors[ImGuiCol_Border] = Border;
	style.Colors[ImGuiCol_BorderShadow] = Alpha(Black, 0.0f);
	style.Colors[ImGuiCol_FrameBg] = Surface;
	style.Colors[ImGuiCol_FrameBgHovered] = Blend(Surface, AccentColor, 0.25f);
	style.Colors[ImGuiCol_FrameBgActive] = Blend(Surface, AccentColor, 0.55f);
	style.Colors[ImGuiCol_TitleBg] = Recessed;
	style.Colors[ImGuiCol_TitleBgActive] = Blend(BackgroundColor, AccentColor, 0.22f);
	style.Colors[ImGuiCol_TitleBgCollapsed] = Recessed;
	style.Colors[ImGuiCol_MenuBarBg] = Recessed;
	style.Colors[ImGuiCol_ScrollbarBg] = Recessed;
	style.Colors[ImGuiCol_ScrollbarGrab] = Blend(BackgroundColor, TextColor, 0.28f);
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = YellowButtonHovered;
	style.Colors[ImGuiCol_ScrollbarGrabActive] = YellowButtonActive;
	style.Colors[ImGuiCol_CheckMark] = AccentHighlight;
	style.Colors[ImGuiCol_SliderGrab] = AccentColor;
	style.Colors[ImGuiCol_SliderGrabActive] = AccentHighlight;
	style.Colors[ImGuiCol_Button] = YellowButton;
	style.Colors[ImGuiCol_ButtonHovered] = YellowButtonHovered;
	style.Colors[ImGuiCol_ButtonActive] = YellowButtonActive;
	style.Colors[ImGuiCol_Header] = Blend(BackgroundColor, AccentColor, 0.28f);
	style.Colors[ImGuiCol_HeaderHovered] = YellowButtonHovered;
	style.Colors[ImGuiCol_HeaderActive] = Selected;
	style.Colors[ImGuiCol_Separator] = Border;
	style.Colors[ImGuiCol_SeparatorHovered] = AccentColor;
	style.Colors[ImGuiCol_SeparatorActive] = AccentHighlight;
	style.Colors[ImGuiCol_ResizeGrip] = Alpha(AccentColor, 0.35f);
	style.Colors[ImGuiCol_ResizeGripHovered] = Alpha(AccentHighlight, 0.70f);
	style.Colors[ImGuiCol_ResizeGripActive] = AccentHighlight;
	style.Colors[ImGuiCol_InputTextCursor] = TextColor;
	style.Colors[ImGuiCol_TabHovered] = YellowButtonHovered;
	style.Colors[ImGuiCol_Tab] = Surface;
	style.Colors[ImGuiCol_TabSelected] = Selected;
	style.Colors[ImGuiCol_TabSelectedOverline] = SelectedBorder;
	style.Colors[ImGuiCol_TabDimmed] = Recessed;
	style.Colors[ImGuiCol_TabDimmedSelected] = Blend(BackgroundColor, Selected, 0.50f);
	style.Colors[ImGuiCol_TabDimmedSelectedOverline] = Blend(Border, SelectedBorder, 0.45f);
	style.Colors[ImGuiCol_DockingPreview] = Alpha(AccentColor, 0.65f);
	style.Colors[ImGuiCol_DockingEmptyBg] = Recessed;
	style.Colors[ImGuiCol_PlotLines] = Blend(TextColor, AccentColor, 0.25f);
	style.Colors[ImGuiCol_PlotLinesHovered] = AccentHighlight;
	style.Colors[ImGuiCol_PlotHistogram] = AccentColor;
	style.Colors[ImGuiCol_PlotHistogramHovered] = AccentHighlight;
	style.Colors[ImGuiCol_TableHeaderBg] = Raised;
	style.Colors[ImGuiCol_TableBorderStrong] = Border;
	style.Colors[ImGuiCol_TableBorderLight] = Blend(BackgroundColor, Border, 0.50f);
	style.Colors[ImGuiCol_TableRowBg] = Alpha(BackgroundColor, 0.0f);
	style.Colors[ImGuiCol_TableRowBgAlt] = Alpha(TextColor, 0.035f);
	style.Colors[ImGuiCol_TextLink] = AccentHighlight;
	style.Colors[ImGuiCol_TextSelectedBg] = Alpha(AccentColor, 0.50f);
	style.Colors[ImGuiCol_TreeLines] = Border;
	style.Colors[ImGuiCol_DragDropTarget] = AccentHighlight;
	style.Colors[ImGuiCol_DragDropTargetBg] = Alpha(AccentColor, 0.15f);
	style.Colors[ImGuiCol_UnsavedMarker] = AccentHighlight;
	style.Colors[ImGuiCol_NavCursor] = SelectedBorder;
	style.Colors[ImGuiCol_NavWindowingHighlight] = Alpha(TextColor, 0.70f);
	style.Colors[ImGuiCol_NavWindowingDimBg] = Alpha(Black, 0.35f);
	style.Colors[ImGuiCol_ModalWindowDimBg] = Alpha(Black, 0.55f);
}
}
