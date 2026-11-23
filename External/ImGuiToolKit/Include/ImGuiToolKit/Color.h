#pragma once

#include <imgui.h>
#include <IconsFontAwesome5.h>
#include <string>

namespace ImGuiToolKit
{
inline bool ColorEdit3(const char* label, float color[3], ImGuiColorEditFlags flags = 0)
{
	const std::string title = std::string(ICON_FA_PALETTE " ") + label;
	return ImGui::ColorEdit3(title.c_str(), color, flags);
}

inline bool ColorEdit4(const char* label, float color[4], ImGuiColorEditFlags flags = 0)
{
	const std::string title = std::string(ICON_FA_PALETTE " ") + label;
	return ImGui::ColorEdit4(title.c_str(), color, flags);
}

inline bool ColorPicker3(const char* label, float color[3], ImGuiColorEditFlags flags = 0)
{
	const std::string title = std::string(ICON_FA_PALETTE " ") + label;
	return ImGui::ColorPicker3(title.c_str(), color, flags);
}

inline bool ColorPicker4(const char* label, float color[4], ImGuiColorEditFlags flags = 0,
	const float* referenceColor = nullptr)
{
	const std::string title = std::string(ICON_FA_PALETTE " ") + label;
	return ImGui::ColorPicker4(title.c_str(), color, flags, referenceColor);
}

}