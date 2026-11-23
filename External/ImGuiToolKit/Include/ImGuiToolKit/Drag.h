#pragma once

#include <imgui.h>
#include <IconsFontAwesome5.h>
#include <string>

namespace ImGuiToolKit
{
inline bool DragTextSizeFloat(const char* label, float* value, float speed = 1.0f,
	float min = 0.0f, float max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
	const std::string title = std::string(ICON_FA_TEXT_HEIGHT " ") + label;
	return ImGui::DragFloat(title.c_str(), value, speed, min, max, format, flags);
}

inline bool DragDelayFloat(const char* label, float* value, float speed = 1.0f,
	float min = 0.0f, float max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
	const std::string title = std::string(ICON_FA_CLOCK " ") + label;
	return ImGui::DragFloat(title.c_str(), value, speed, min, max, format, flags);
}

inline bool DragSpeedFloat(const char* label, float* value, float speed = 1.0f,
	float min = 0.0f, float max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
	const std::string title = std::string(ICON_FA_RUNNING " ") + label;
	return ImGui::DragFloat(title.c_str(), value, speed, min, max, format, flags);
}

inline bool DragSpeedFloat2(const char* label, float* value, float speed = 1.0f,
	float min = 0.0f, float max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
	const std::string title = std::string(ICON_FA_RUNNING " ") + label;
	return ImGui::DragFloat2(title.c_str(), value, speed, min, max, format, flags);
}

inline bool DragSpeedFloat3(const char* label, float* value, float speed = 1.0f,
	float min = 0.0f, float max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
	const std::string title = std::string(ICON_FA_RUNNING " ") + label;
	return ImGui::DragFloat3(title.c_str(), value, speed, min, max, format, flags);
}

inline bool DragPosFloat(const char* label, float* value, float speed = 1.0f,
	float min = 0.0f, float max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
	const std::string title = std::string(ICON_FA_ARROWS_ALT " ") + label;
	return ImGui::DragFloat(title.c_str(), value, speed, min, max, format, flags);
}

inline bool DragPosFloat2(const char* label, float* value, float speed = 1.0f,
	float min = 0.0f, float max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
	const std::string title = std::string(ICON_FA_ARROWS_ALT " ") + label;
	return ImGui::DragFloat2(title.c_str(), value, speed, min, max, format, flags);
}

inline bool DragPosFloat3(const char* label, float* value, float speed = 1.0f,
	float min = 0.0f, float max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
	const std::string title = std::string(ICON_FA_ARROWS_ALT " ") + label;
	return ImGui::DragFloat3(title.c_str(), value, speed, min, max, format, flags);
}

inline bool DragRotFloat(const char* label, float* value, float speed = 1.0f,
	float min = 0.0f, float max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
	const std::string title = std::string(ICON_FA_SYNC " ") + label;
	return ImGui::DragFloat(title.c_str(), value, speed, min, max, format, flags);
}

inline bool DragRotFloat2(const char* label, float* value, float speed = 1.0f,
	float min = 0.0f, float max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
	const std::string title = std::string(ICON_FA_SYNC " ") + label;
	return ImGui::DragFloat2(title.c_str(), value, speed, min, max, format, flags);
}

inline bool DragRotFloat3(const char* label, float* value, float speed = 1.0f,
	float min = 0.0f, float max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
	const std::string title = std::string(ICON_FA_SYNC " ") + label;
	return ImGui::DragFloat3(title.c_str(), value, speed, min, max, format, flags);
}

inline bool DragSizeFloat(const char* label, float* value, float speed = 1.0f,
	float min = 0.0f, float max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
	const std::string title = std::string(ICON_FA_EXPAND_ARROWS_ALT " ") + label;
	return ImGui::DragFloat(title.c_str(), value, speed, min, max, format, flags);
}

inline bool DragSizeFloat2(const char* label, float* value, float speed = 1.0f,
	float min = 0.0f, float max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
	const std::string title = std::string(ICON_FA_EXPAND_ARROWS_ALT " ") + label;
	return ImGui::DragFloat2(title.c_str(), value, speed, min, max, format, flags);
}

inline bool DragSizeFloat3(const char* label, float* value, float speed = 1.0f,
	float min = 0.0f, float max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
	const std::string title = std::string(ICON_FA_EXPAND_ARROWS_ALT " ") + label;
	return ImGui::DragFloat3(title.c_str(), value, speed, min, max, format, flags);
}


}
