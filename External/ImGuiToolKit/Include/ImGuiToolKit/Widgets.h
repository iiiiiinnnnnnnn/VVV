#pragma once

#include <ImGuiToolKit/Utf8.h>
#include <imgui.h>
#include <imgui_stdlib.h>

namespace ImGuiToolKit
{
inline bool Begin(Utf8Text name, bool* p_open = nullptr, ImGuiWindowFlags flags = 0)
{
	return ImGui::Begin(name.Get(), p_open, flags);
}

inline bool BeginChild(Utf8Text str_id, const ImVec2& size = ImVec2(0, 0), ImGuiChildFlags child_flags = 0, ImGuiWindowFlags window_flags = 0)
{
	return ImGui::BeginChild(str_id.Get(), size, child_flags, window_flags);
}

inline void PushID(Utf8Text str_id)
{
	return ImGui::PushID(str_id.Get());
}

inline void PushID(Utf8Text str_id_begin, Utf8Text str_id_end)
{
	return ImGui::PushID(str_id_begin.Get(), str_id_end.Get());
}

inline ImGuiID GetID(Utf8Text str_id)
{
	return ImGui::GetID(str_id.Get());
}

inline ImGuiID GetID(Utf8Text str_id_begin, Utf8Text str_id_end)
{
	return ImGui::GetID(str_id_begin.Get(), str_id_end.Get());
}

inline void TextUnformatted(Utf8Text text, Utf8Text text_end = nullptr)
{
	return ImGui::TextUnformatted(text.Get(), text_end.Get());
}

inline void SeparatorText(Utf8Text label)
{
	return ImGui::SeparatorText(label.Get());
}

inline bool Button(Utf8Text label, const ImVec2& size = ImVec2(0, 0))
{
	return ImGui::Button(label.Get(), size);
}

inline bool SmallButton(Utf8Text label)
{
	return ImGui::SmallButton(label.Get());
}

inline bool InvisibleButton(Utf8Text str_id, const ImVec2& size, ImGuiButtonFlags flags = 0)
{
	return ImGui::InvisibleButton(str_id.Get(), size, flags);
}

inline bool ArrowButton(Utf8Text str_id, ImGuiDir dir)
{
	return ImGui::ArrowButton(str_id.Get(), dir);
}

inline bool Checkbox(Utf8Text label, bool* v)
{
	return ImGui::Checkbox(label.Get(), v);
}

inline bool CheckboxFlags(Utf8Text label, int* flags, int flags_value)
{
	return ImGui::CheckboxFlags(label.Get(), flags, flags_value);
}

inline bool CheckboxFlags(Utf8Text label, unsigned int* flags, unsigned int flags_value)
{
	return ImGui::CheckboxFlags(label.Get(), flags, flags_value);
}

inline bool RadioButton(Utf8Text label, bool active)
{
	return ImGui::RadioButton(label.Get(), active);
}

inline bool RadioButton(Utf8Text label, int* v, int v_button)
{
	return ImGui::RadioButton(label.Get(), v, v_button);
}

inline bool BeginCombo(Utf8Text label, Utf8Text preview_value, ImGuiComboFlags flags = 0)
{
	return ImGui::BeginCombo(label.Get(), preview_value.Get(), flags);
}

inline bool Combo(Utf8Text label, int* current_item, const char* (*getter)(void*, int),
	void* user_data, int items_count, int popup_max_height_in_items = -1)
{
	return ImGui::Combo(label.Get(), current_item, getter, user_data, items_count, popup_max_height_in_items);
}

inline bool Combo(Utf8Text label, int* current_item, const char* const items[], int items_count, int popup_max_height_in_items = -1)
{
	return ImGui::Combo(label.Get(), current_item, items, items_count, popup_max_height_in_items);
}

inline bool Combo(Utf8Text label, int* current_item, Utf8Text items_separated_by_zeros, int popup_max_height_in_items = -1)
{
	return ImGui::Combo(label.Get(), current_item, items_separated_by_zeros.Get(), popup_max_height_in_items);
}

inline bool DragFloat(Utf8Text label, float* v, float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, Utf8Text format = "%.3f", ImGuiSliderFlags flags = 0)
{
	return ImGui::DragFloat(label.Get(), v, v_speed, v_min, v_max, format.Get(), flags);
}

inline bool DragFloat2(Utf8Text label, float v[2], float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, Utf8Text format = "%.3f", ImGuiSliderFlags flags = 0)
{
	return ImGui::DragFloat2(label.Get(), v, v_speed, v_min, v_max, format.Get(), flags);
}

inline bool DragFloat3(Utf8Text label, float v[3], float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, Utf8Text format = "%.3f", ImGuiSliderFlags flags = 0)
{
	return ImGui::DragFloat3(label.Get(), v, v_speed, v_min, v_max, format.Get(), flags);
}

inline bool DragFloat4(Utf8Text label, float v[4], float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, Utf8Text format = "%.3f", ImGuiSliderFlags flags = 0)
{
	return ImGui::DragFloat4(label.Get(), v, v_speed, v_min, v_max, format.Get(), flags);
}

inline bool DragFloatRange2(Utf8Text label, float* v_current_min, float* v_current_max, float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, Utf8Text format = "%.3f", Utf8Text format_max = nullptr, ImGuiSliderFlags flags = 0)
{
	return ImGui::DragFloatRange2(label.Get(), v_current_min, v_current_max, v_speed, v_min, v_max, format.Get(), format_max.Get(), flags);
}

inline bool DragInt(Utf8Text label, int* v, float v_speed = 1.0f, int v_min = 0, int v_max = 0, Utf8Text format = "%d", ImGuiSliderFlags flags = 0)
{
	return ImGui::DragInt(label.Get(), v, v_speed, v_min, v_max, format.Get(), flags);
}

inline bool DragInt2(Utf8Text label, int v[2], float v_speed = 1.0f, int v_min = 0, int v_max = 0, Utf8Text format = "%d", ImGuiSliderFlags flags = 0)
{
	return ImGui::DragInt2(label.Get(), v, v_speed, v_min, v_max, format.Get(), flags);
}

inline bool DragInt3(Utf8Text label, int v[3], float v_speed = 1.0f, int v_min = 0, int v_max = 0, Utf8Text format = "%d", ImGuiSliderFlags flags = 0)
{
	return ImGui::DragInt3(label.Get(), v, v_speed, v_min, v_max, format.Get(), flags);
}

inline bool DragInt4(Utf8Text label, int v[4], float v_speed = 1.0f, int v_min = 0, int v_max = 0, Utf8Text format = "%d", ImGuiSliderFlags flags = 0)
{
	return ImGui::DragInt4(label.Get(), v, v_speed, v_min, v_max, format.Get(), flags);
}

inline bool DragIntRange2(Utf8Text label, int* v_current_min, int* v_current_max, float v_speed = 1.0f, int v_min = 0, int v_max = 0, Utf8Text format = "%d", Utf8Text format_max = nullptr, ImGuiSliderFlags flags = 0)
{
	return ImGui::DragIntRange2(label.Get(), v_current_min, v_current_max, v_speed, v_min, v_max, format.Get(), format_max.Get(), flags);
}

inline bool DragScalar(Utf8Text label, ImGuiDataType data_type, void* p_data, float v_speed = 1.0f, const void* p_min = nullptr, const void* p_max = nullptr, Utf8Text format = nullptr, ImGuiSliderFlags flags = 0)
{
	return ImGui::DragScalar(label.Get(), data_type, p_data, v_speed, p_min, p_max, format.Get(), flags);
}

inline bool DragScalarN(Utf8Text label, ImGuiDataType data_type, void* p_data, int components, float v_speed = 1.0f, const void* p_min = nullptr, const void* p_max = nullptr, Utf8Text format = nullptr, ImGuiSliderFlags flags = 0)
{
	return ImGui::DragScalarN(label.Get(), data_type, p_data, components, v_speed, p_min, p_max, format.Get(), flags);
}

inline bool SliderFloat(Utf8Text label, float* v, float v_min, float v_max, Utf8Text format = "%.3f", ImGuiSliderFlags flags = 0)
{
	return ImGui::SliderFloat(label.Get(), v, v_min, v_max, format.Get(), flags);
}

inline bool SliderFloat2(Utf8Text label, float v[2], float v_min, float v_max, Utf8Text format = "%.3f", ImGuiSliderFlags flags = 0)
{
	return ImGui::SliderFloat2(label.Get(), v, v_min, v_max, format.Get(), flags);
}

inline bool SliderFloat3(Utf8Text label, float v[3], float v_min, float v_max, Utf8Text format = "%.3f", ImGuiSliderFlags flags = 0)
{
	return ImGui::SliderFloat3(label.Get(), v, v_min, v_max, format.Get(), flags);
}

inline bool SliderFloat4(Utf8Text label, float v[4], float v_min, float v_max, Utf8Text format = "%.3f", ImGuiSliderFlags flags = 0)
{
	return ImGui::SliderFloat4(label.Get(), v, v_min, v_max, format.Get(), flags);
}

inline bool SliderAngle(Utf8Text label, float* v_rad, float v_degrees_min = -360.0f, float v_degrees_max = +360.0f, Utf8Text format = "%.0f deg", ImGuiSliderFlags flags = 0)
{
	return ImGui::SliderAngle(label.Get(), v_rad, v_degrees_min, v_degrees_max, format.Get(), flags);
}

inline bool SliderInt(Utf8Text label, int* v, int v_min, int v_max, Utf8Text format = "%d", ImGuiSliderFlags flags = 0)
{
	return ImGui::SliderInt(label.Get(), v, v_min, v_max, format.Get(), flags);
}

inline bool SliderInt2(Utf8Text label, int v[2], int v_min, int v_max, Utf8Text format = "%d", ImGuiSliderFlags flags = 0)
{
	return ImGui::SliderInt2(label.Get(), v, v_min, v_max, format.Get(), flags);
}

inline bool SliderInt3(Utf8Text label, int v[3], int v_min, int v_max, Utf8Text format = "%d", ImGuiSliderFlags flags = 0)
{
	return ImGui::SliderInt3(label.Get(), v, v_min, v_max, format.Get(), flags);
}

inline bool SliderInt4(Utf8Text label, int v[4], int v_min, int v_max, Utf8Text format = "%d", ImGuiSliderFlags flags = 0)
{
	return ImGui::SliderInt4(label.Get(), v, v_min, v_max, format.Get(), flags);
}

inline bool SliderScalar(Utf8Text label, ImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max, Utf8Text format = nullptr, ImGuiSliderFlags flags = 0)
{
	return ImGui::SliderScalar(label.Get(), data_type, p_data, p_min, p_max, format.Get(), flags);
}

inline bool SliderScalarN(Utf8Text label, ImGuiDataType data_type, void* p_data, int components, const void* p_min, const void* p_max, Utf8Text format = nullptr, ImGuiSliderFlags flags = 0)
{
	return ImGui::SliderScalarN(label.Get(), data_type, p_data, components, p_min, p_max, format.Get(), flags);
}

inline bool VSliderFloat(Utf8Text label, const ImVec2& size, float* v, float v_min, float v_max, Utf8Text format = "%.3f", ImGuiSliderFlags flags = 0)
{
	return ImGui::VSliderFloat(label.Get(), size, v, v_min, v_max, format.Get(), flags);
}

inline bool VSliderInt(Utf8Text label, const ImVec2& size, int* v, int v_min, int v_max, Utf8Text format = "%d", ImGuiSliderFlags flags = 0)
{
	return ImGui::VSliderInt(label.Get(), size, v, v_min, v_max, format.Get(), flags);
}

inline bool VSliderScalar(Utf8Text label, const ImVec2& size, ImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max, Utf8Text format = nullptr, ImGuiSliderFlags flags = 0)
{
	return ImGui::VSliderScalar(label.Get(), size, data_type, p_data, p_min, p_max, format.Get(), flags);
}

inline bool InputText(Utf8Text label, char* buf, size_t buf_size, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = nullptr, void* user_data = nullptr)
{
	return ImGui::InputText(label.Get(), buf, buf_size, flags, callback, user_data);
}

inline bool InputTextMultiline(Utf8Text label, char* buf, size_t buf_size, const ImVec2& size = ImVec2(0, 0), ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = nullptr, void* user_data = nullptr)
{
	return ImGui::InputTextMultiline(label.Get(), buf, buf_size, size, flags, callback, user_data);
}

inline bool InputTextWithHint(Utf8Text label, Utf8Text hint, char* buf, size_t buf_size, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = nullptr, void* user_data = nullptr)
{
	return ImGui::InputTextWithHint(label.Get(), hint.Get(), buf, buf_size, flags, callback, user_data);
}

inline bool InputFloat(Utf8Text label, float* v, float step = 0.0f, float step_fast = 0.0f, Utf8Text format = "%.3f", ImGuiInputTextFlags flags = 0)
{
	return ImGui::InputFloat(label.Get(), v, step, step_fast, format.Get(), flags);
}

inline bool InputFloat2(Utf8Text label, float v[2], Utf8Text format = "%.3f", ImGuiInputTextFlags flags = 0)
{
	return ImGui::InputFloat2(label.Get(), v, format.Get(), flags);
}

inline bool InputFloat3(Utf8Text label, float v[3], Utf8Text format = "%.3f", ImGuiInputTextFlags flags = 0)
{
	return ImGui::InputFloat3(label.Get(), v, format.Get(), flags);
}

inline bool InputFloat4(Utf8Text label, float v[4], Utf8Text format = "%.3f", ImGuiInputTextFlags flags = 0)
{
	return ImGui::InputFloat4(label.Get(), v, format.Get(), flags);
}

inline bool InputInt(Utf8Text label, int* v, int step = 1, int step_fast = 100, ImGuiInputTextFlags flags = 0)
{
	return ImGui::InputInt(label.Get(), v, step, step_fast, flags);
}

inline bool InputInt2(Utf8Text label, int v[2], ImGuiInputTextFlags flags = 0)
{
	return ImGui::InputInt2(label.Get(), v, flags);
}

inline bool InputInt3(Utf8Text label, int v[3], ImGuiInputTextFlags flags = 0)
{
	return ImGui::InputInt3(label.Get(), v, flags);
}

inline bool InputInt4(Utf8Text label, int v[4], ImGuiInputTextFlags flags = 0)
{
	return ImGui::InputInt4(label.Get(), v, flags);
}

inline bool InputDouble(Utf8Text label, double* v, double step = 0.0, double step_fast = 0.0, Utf8Text format = "%.6f", ImGuiInputTextFlags flags = 0)
{
	return ImGui::InputDouble(label.Get(), v, step, step_fast, format.Get(), flags);
}

inline bool InputScalar(Utf8Text label, ImGuiDataType data_type, void* p_data, const void* p_step = nullptr, const void* p_step_fast = nullptr, Utf8Text format = nullptr, ImGuiInputTextFlags flags = 0)
{
	return ImGui::InputScalar(label.Get(), data_type, p_data, p_step, p_step_fast, format.Get(), flags);
}

inline bool InputScalarN(Utf8Text label, ImGuiDataType data_type, void* p_data, int components, const void* p_step = nullptr, const void* p_step_fast = nullptr, Utf8Text format = nullptr, ImGuiInputTextFlags flags = 0)
{
	return ImGui::InputScalarN(label.Get(), data_type, p_data, components, p_step, p_step_fast, format.Get(), flags);
}

inline bool TreeNode(Utf8Text label)
{
	return ImGui::TreeNode(label.Get());
}

inline bool TreeNodeEx(Utf8Text label, ImGuiTreeNodeFlags flags = 0)
{
	return ImGui::TreeNodeEx(label.Get(), flags);
}

inline void TreePush(Utf8Text str_id)
{
	return ImGui::TreePush(str_id.Get());
}

inline bool CollapsingHeader(Utf8Text label, ImGuiTreeNodeFlags flags = 0)
{
	return ImGui::CollapsingHeader(label.Get(), flags);
}

inline bool CollapsingHeader(Utf8Text label, bool* p_visible, ImGuiTreeNodeFlags flags = 0)
{
	return ImGui::CollapsingHeader(label.Get(), p_visible, flags);
}

inline bool Selectable(Utf8Text label, bool selected = false, ImGuiSelectableFlags flags = 0, const ImVec2& size = ImVec2(0, 0))
{
	return ImGui::Selectable(label.Get(), selected, flags, size);
}

inline bool Selectable(Utf8Text label, bool* p_selected, ImGuiSelectableFlags flags = 0, const ImVec2& size = ImVec2(0, 0))
{
	return ImGui::Selectable(label.Get(), p_selected, flags, size);
}

inline bool BeginMenu(Utf8Text label, bool enabled = true)
{
	return ImGui::BeginMenu(label.Get(), enabled);
}

inline bool MenuItem(Utf8Text label, Utf8Text shortcut = nullptr, bool selected = false, bool enabled = true)
{
	return ImGui::MenuItem(label.Get(), shortcut.Get(), selected, enabled);
}

inline bool MenuItem(Utf8Text label, Utf8Text shortcut, bool* p_selected, bool enabled = true)
{
	return ImGui::MenuItem(label.Get(), shortcut.Get(), p_selected, enabled);
}

inline bool BeginPopup(Utf8Text str_id, ImGuiWindowFlags flags = 0)
{
	return ImGui::BeginPopup(str_id.Get(), flags);
}

inline bool BeginPopupModal(Utf8Text name, bool* p_open = nullptr, ImGuiWindowFlags flags = 0)
{
	return ImGui::BeginPopupModal(name.Get(), p_open, flags);
}

inline void OpenPopup(Utf8Text str_id, ImGuiPopupFlags popup_flags = 0)
{
	return ImGui::OpenPopup(str_id.Get(), popup_flags);
}

inline bool BeginTable(Utf8Text str_id, int columns, ImGuiTableFlags flags = 0, const ImVec2& outer_size = ImVec2(0.0f, 0.0f), float inner_width = 0.0f)
{
	return ImGui::BeginTable(str_id.Get(), columns, flags, outer_size, inner_width);
}

inline void TableSetupColumn(Utf8Text label, ImGuiTableColumnFlags flags = 0, float init_width_or_weight = 0.0f, ImGuiID user_id = 0)
{
	return ImGui::TableSetupColumn(label.Get(), flags, init_width_or_weight, user_id);
}

inline bool BeginTabBar(Utf8Text str_id, ImGuiTabBarFlags flags = 0)
{
	return ImGui::BeginTabBar(str_id.Get(), flags);
}

inline bool BeginTabItem(Utf8Text label, bool* p_open = nullptr, ImGuiTabItemFlags flags = 0)
{
	return ImGui::BeginTabItem(label.Get(), p_open, flags);
}

inline void SetClipboardText(Utf8Text text)
{
	return ImGui::SetClipboardText(text.Get());
}

inline bool InputText(Utf8Text label, std::string* str, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = nullptr, void* user_data = nullptr)
{
	return ImGui::InputText(label.Get(), str, flags, callback, user_data);
}

inline bool InputTextMultiline(Utf8Text label, std::string* str, const ImVec2& size = ImVec2(0, 0), ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = nullptr, void* user_data = nullptr)
{
	return ImGui::InputTextMultiline(label.Get(), str, size, flags, callback, user_data);
}

inline bool InputTextWithHint(Utf8Text label, Utf8Text hint, std::string* str, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = nullptr, void* user_data = nullptr)
{
	return ImGui::InputTextWithHint(label.Get(), hint.Get(), str, flags, callback, user_data);
}

template<class... Args>
inline void Text(Utf8Text format, Args... args)
{
	ImGui::Text(format.Get(), args...);
}

template<class... Args>
inline void TextDisabled(Utf8Text format, Args... args)
{
	ImGui::TextDisabled(format.Get(), args...);
}

template<class... Args>
inline void TextWrapped(Utf8Text format, Args... args)
{
	ImGui::TextWrapped(format.Get(), args...);
}

template<class... Args>
inline void SetTooltip(Utf8Text format, Args... args)
{
	ImGui::SetTooltip(format.Get(), args...);
}

template<class... Args>
inline void SetItemTooltip(Utf8Text format, Args... args)
{
	ImGui::SetItemTooltip(format.Get(), args...);
}

template<class... Args>
inline void BulletText(Utf8Text format, Args... args)
{
	ImGui::BulletText(format.Get(), args...);
}

template<class... Args>
inline void TextColored(const ImVec4& color, Utf8Text format, Args... args)
{
	ImGui::TextColored(color, format.Get(), args...);
}

template<class... Args>
inline bool TreeNode(Utf8Text id, Utf8Text format, Args... args)
{
	return ImGui::TreeNode(id.Get(), format.Get(), args...);
}

template<class... Args>
inline bool TreeNodeEx(Utf8Text id, ImGuiTreeNodeFlags flags, Utf8Text format, Args... args)
{
	return ImGui::TreeNodeEx(id.Get(), flags, format.Get(), args...);
}

}
