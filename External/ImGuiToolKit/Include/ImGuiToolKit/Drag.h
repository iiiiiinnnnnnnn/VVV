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

inline bool DragTextSizeFloat(const char8_t* label, float* value, float speed = 1.0f,
	float min = 0.0f, float max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
	return DragTextSizeFloat(reinterpret_cast<const char*>(label), value, speed, min, max, format, flags);
}

inline bool DragDelayFloat(const char8_t* label, float* value, float speed = 1.0f,
	float min = 0.0f, float max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
	return DragDelayFloat(reinterpret_cast<const char*>(label), value, speed, min, max, format, flags);
}

inline bool DragSpeedFloat(const char8_t* label, float* value, float speed = 1.0f,
	float min = 0.0f, float max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
	return DragSpeedFloat(reinterpret_cast<const char*>(label), value, speed, min, max, format, flags);
}

inline bool DragSpeedFloat2(const char8_t* label, float* value, float speed = 1.0f,
	float min = 0.0f, float max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
	return DragSpeedFloat2(reinterpret_cast<const char*>(label), value, speed, min, max, format, flags);
}

inline bool DragSpeedFloat3(const char8_t* label, float* value, float speed = 1.0f,
	float min = 0.0f, float max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
	return DragSpeedFloat3(reinterpret_cast<const char*>(label), value, speed, min, max, format, flags);
}

inline bool DragPosFloat(const char8_t* label, float* value, float speed = 1.0f,
	float min = 0.0f, float max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
	return DragPosFloat(reinterpret_cast<const char*>(label), value, speed, min, max, format, flags);
}

inline bool DragPosFloat2(const char8_t* label, float* value, float speed = 1.0f,
	float min = 0.0f, float max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
	return DragPosFloat2(reinterpret_cast<const char*>(label), value, speed, min, max, format, flags);
}

inline bool DragPosFloat3(const char8_t* label, float* value, float speed = 1.0f,
	float min = 0.0f, float max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
	return DragPosFloat3(reinterpret_cast<const char*>(label), value, speed, min, max, format, flags);
}

inline bool DragRotFloat(const char8_t* label, float* value, float speed = 1.0f,
	float min = 0.0f, float max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
	return DragRotFloat(reinterpret_cast<const char*>(label), value, speed, min, max, format, flags);
}

inline bool DragRotFloat2(const char8_t* label, float* value, float speed = 1.0f,
	float min = 0.0f, float max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
	return DragRotFloat2(reinterpret_cast<const char*>(label), value, speed, min, max, format, flags);
}

inline bool DragRotFloat3(const char8_t* label, float* value, float speed = 1.0f,
	float min = 0.0f, float max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
	return DragRotFloat3(reinterpret_cast<const char*>(label), value, speed, min, max, format, flags);
}

inline bool DragSizeFloat(const char8_t* label, float* value, float speed = 1.0f,
	float min = 0.0f, float max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
	return DragSizeFloat(reinterpret_cast<const char*>(label), value, speed, min, max, format, flags);
}

inline bool DragSizeFloat2(const char8_t* label, float* value, float speed = 1.0f,
	float min = 0.0f, float max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
	return DragSizeFloat2(reinterpret_cast<const char*>(label), value, speed, min, max, format, flags);
}

inline bool DragSizeFloat3(const char8_t* label, float* value, float speed = 1.0f,
	float min = 0.0f, float max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
	return DragSizeFloat3(reinterpret_cast<const char*>(label), value, speed, min, max, format, flags);
}


}
