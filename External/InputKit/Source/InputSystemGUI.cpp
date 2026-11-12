#include <ImGuiToolKit/ImGuiToolKit.h>
#include <InputKit/InputSystemGUI.h>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <algorithm>

namespace InputKit
{
static const char* devices[] = {JText(u8"キーボード"), JText(u8"マウス"), JText(u8"ゲームパッド"), JText(u8"軸")};
static const char* axes[] = {JText(u8"なし"), JText(u8"左スティック X"), JText(u8"左スティック Y"), JText(u8"右スティック X"), JText(u8"右スティック Y")};
static const char* directions[] = {JText(u8"左"), JText(u8"右"), JText(u8"後ろ"), JText(u8"前")};
static const char* padButtons[] = {JText(u8"十字キー 上"), JText(u8"十字キー 右"), JText(u8"十字キー 下"), JText(u8"十字キー 左"),
	"A", "B", "X", "Y", JText(u8"スタート"), JText(u8"バック"), JText(u8"左スティック押し込み"), JText(u8"右スティック押し込み"),
	JText(u8"左ショルダー"), JText(u8"右ショルダー"), JText(u8"左トリガー"), JText(u8"右トリガー")};

static std::string GetKeyName(int code)
{
	wchar_t text[128]{};
	UINT scan = MapVirtualKeyW(static_cast<UINT>(code), MAPVK_VK_TO_VSC);
	if (code == VK_LEFT || code == VK_RIGHT || code == VK_UP || code == VK_DOWN ||
		code == VK_INSERT || code == VK_DELETE || code == VK_HOME || code == VK_END ||
		code == VK_PRIOR || code == VK_NEXT || code == VK_RCONTROL || code == VK_RMENU) scan |= 0x100;
	if (!GetKeyNameTextW(static_cast<LONG>(scan << 16), text, 128)) return std::string("VK ") + std::to_string(code);
	const int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
	if (size <= 0) return std::string("VK ") + std::to_string(code);
	std::string result(static_cast<size_t>(size), '\0');
	WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), size, nullptr, nullptr);
	result.pop_back();
	return result;
}

static std::string GetBindingName(const Binding& binding)
{
	switch (binding.device)
	{
	case Device::Keyboard: return GetKeyName(binding.code) + JText(u8" [キーボード]");
	case Device::Mouse:
		if (binding.code == Mouse::BTN_LEFT) return JText(u8"左 [マウス]");
		if (binding.code == Mouse::BTN_MIDDLE) return JText(u8"中央 [マウス]");
		return JText(u8"右 [マウス]");
	case Device::GamePad:
		for (int i = 0; i < 16; ++i)
		{
			if (binding.code == (1 << i)) return std::string(padButtons[i]) + JText(u8" [ゲームパッド]");
		}
		return std::string(JText(u8"ボタンマスク ")) + std::to_string(binding.code) + JText(u8" [ゲームパッド]");
	case Device::Axis:
		return std::string(axes[std::clamp(static_cast<int>(binding.axis), 0, 4)]) + (binding.negative ? JText(u8" - [軸]") : JText(u8" + [軸]"));
	}
	return std::string(JText(u8"不明なバインディング"));
}

static std::vector<std::string> GetMapNames(const InputActions& actions)
{
	std::vector<std::string> names;
	for (const auto& [name, map] : actions.GetMaps()) names.push_back(name);
	std::sort(names.begin(), names.end());
	return names;
}

static std::vector<std::string> GetActionNames(const InputActionMap& map)
{
	std::vector<std::string> names;
	for (const auto& [name, action] : map.GetActions()) names.push_back(name);
	std::sort(names.begin(), names.end());
	return names;
}

void InputSystemGUI::DrawGUI(InputSystem& system)
{
	ImGui::PushID(this);
	DrawFileControls(system);

	auto& actions = system.GetActions();
	if (!actions.FindMap(selectedMap))
	{
		const auto names = GetMapNames(actions);
		selectedMap = names.empty() ? "" : names.front();
		selectedAction.clear();
	}

	if (ImGui::BeginTable("Input Editor", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
	{
		ImGuiToolKit::TableSetupColumn(u8"アクションマップ", ImGuiTableColumnFlags_WidthStretch, 0.7f);
		ImGuiToolKit::TableSetupColumn(u8"アクション", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGuiToolKit::TableSetupColumn(u8"詳細", ImGuiTableColumnFlags_WidthStretch, 1.3f);
		ImGui::TableNextRow();

		ImGui::TableSetColumnIndex(0);
		ImGui::BeginChild("maps", ImVec2(0, std::max(ImGui::GetContentRegionAvail().y, 280.0f)), ImGuiChildFlags_Borders);
		DrawMaps(system);
		ImGui::EndChild();

		ImGui::TableSetColumnIndex(1);
		ImGui::BeginChild("actions", ImVec2(0, std::max(ImGui::GetContentRegionAvail().y, 280.0f)), ImGuiChildFlags_Borders);
		DrawActions(actions);
		ImGui::EndChild();

		ImGui::TableSetColumnIndex(2);
		ImGui::BeginChild("details", ImVec2(0, std::max(ImGui::GetContentRegionAvail().y, 280.0f)), ImGuiChildFlags_Borders);
		DrawDetails(system);
		ImGui::EndChild();
		ImGui::EndTable();
	}

	if (ImGui::IsAnyItemActive()) system.SuppressGameplayInput();
	ImGui::PopID();
}

void InputSystemGUI::DrawFileControls(InputSystem& system)
{
	const auto& files = system.GetFiles();
	const auto selected = system.GetSelectedFile();
	const std::string preview = files[selected].path.empty() ? JText(u8"（未保存）") : files[selected].path.filename().generic_string();
	if (ImGuiToolKit::BeginCombo(u8"入力ファイル", preview.c_str()))
	{
		for (size_t index = 0; index < files.size(); ++index)
		{
			ImGui::PushID(static_cast<int>(index));
			const std::string name = files[index].path.empty() ? JText(u8"（未保存）") : files[index].path.generic_string();
			if (ImGui::Selectable(name.c_str(), index == selected))
			{
				system.SelectFile(index);
				selectedMap.clear();
				selectedAction.clear();
				editingTarget.clear();
			}
			ImGui::PopID();
		}
		ImGui::EndCombo();
	}

	const auto currentFile = system.GetFilePath().generic_string();
	if (filePathSeen != currentFile)
	{
		filePathSeen = currentFile;
		filePathEdit = currentFile;
	}
	ImGui::SetNextItemWidth(std::max(100.0f, ImGui::GetContentRegionAvail().x - 170.0f));
	ImGui::InputText("##Path", &filePathEdit);
	ImGui::SameLine();
	if (ImGuiToolKit::Button(u8"保存"))
	{
		if (!system.Save(filePathEdit)) ImGuiToolKit::Alert(system.GetLastError(), JText(u8"入力ファイルエラー"));
	}
	ImGui::SameLine();
	if (ImGuiToolKit::Button(u8"読み込み"))
	{
		if (system.Load(filePathEdit))
		{
			selectedMap.clear();
			selectedAction.clear();
			editingTarget.clear();
		}
		else ImGuiToolKit::Alert(system.GetLastError(), JText(u8"入力ファイルエラー"));
	}

}

void InputSystemGUI::DrawMaps(InputSystem& system)
{
	auto& actions = system.GetActions();
	ImGuiToolKit::SeparatorText(u8"アクションマップ");
	if (ImGuiToolKit::Button(u8"+ マップ"))
	{
		std::string name = "New Map";
		for (int i = 2; system.FindActionMap(name); ++i) name = "New Map " + std::to_string(i);
		actions.RegisterMap(name);
		selectedMap = name;
		selectedAction.clear();
	}
	for (const auto& name : GetMapNames(actions))
	{
		if (ImGui::Selectable(name.c_str(), name == selectedMap))
		{
			selectedMap = name;
			selectedAction.clear();
		}
	}
}

void InputSystemGUI::DrawActions(InputActions& actions)
{
	ImGuiToolKit::SeparatorText(u8"アクション");
	auto* map = actions.FindMap(selectedMap);
	if (!map) return;

	if (ImGuiToolKit::Button(u8"+ アクション"))
	{
		std::string name = "New Action";
		for (int i = 2; map->FindAction(name); ++i) name = "New Action " + std::to_string(i);
		map->RegisterAction(name);
		selectedAction = name;
	}
	for (const auto& name : GetActionNames(*map))
	{
		if (ImGui::Selectable(name.c_str(), name == selectedAction)) selectedAction = name;
	}
}

bool InputSystemGUI::DrawBindings(InputSystem& system, const char* label, std::vector<Binding>& bindings)
{
	ImGui::PushID(label);
	ImGui::SeparatorText(label);
	bool changed = false;
	if (ImGuiToolKit::Button(u8"+ バインディング"))
	{
		bindings.push_back(Binding{});
		changed = true;
	}

	int removeIndex = -1;
	for (int index = 0; index < static_cast<int>(bindings.size()); ++index)
	{
		ImGui::PushID(index);
		auto& binding = bindings[index];
		ImGui::SeparatorText(GetBindingName(binding).c_str());
		changed |= DrawBindingEditor(system, binding);
		if (ImGuiToolKit::Button(u8"削除")) removeIndex = index;
		ImGui::PopID();
	}
	if (removeIndex >= 0)
	{
		bindings.erase(bindings.begin() + removeIndex);
		changed = true;
	}
	ImGui::PopID();
	return changed;
}

void InputSystemGUI::DrawDetails(InputSystem& system)
{
	ImGuiToolKit::SeparatorText(u8"詳細");
	auto& actions = system.GetActions();
	auto* map = actions.FindMap(selectedMap);
	if (!map) return;

	auto* action = map->FindAction(selectedAction);
	const std::string currentTarget = selectedMap + "/" + selectedAction;
	if (editingTarget != currentTarget)
	{
		editingTarget = currentTarget;
		editName = action ? selectedAction : selectedMap;
		nameError = false;
	}
	ImGui::SetNextItemWidth(std::max(80.0f, ImGui::GetContentRegionAvail().x - 80.0f));
	if (ImGuiToolKit::InputText(u8"名前", &editName, ImGuiInputTextFlags_EnterReturnsTrue))
	{
		bool renamed = false;
		if (action) renamed = map->RenameAction(selectedAction, editName);
		else renamed = system.RenameMap(selectedMap, editName);
		nameError = !renamed;
		if (renamed)
		{
			if (action) selectedAction = editName;
			else selectedMap = editName;
			map = actions.FindMap(selectedMap);
			action = map->FindAction(selectedAction);
		}
	}
	if (nameError) ImGuiToolKit::TextColored(ImVec4(1, 0.4f, 0.4f, 1), u8"名前は空欄・重複を避け、「/」を含めないでください。");
	if (!action)
	{
		if (ImGuiToolKit::Button(u8"削除"))
		{
			actions.RemoveMap(selectedMap);
			selectedMap.clear();
			selectedAction.clear();
		}
		return;
	}

	ImGui::TextWrapped("%s / %s", selectedMap.c_str(), selectedAction.c_str());
	ImGuiToolKit::TextDisabled(u8"押下中 %d  押した瞬間 %d  離した瞬間 %d", action->state.held, action->state.down, action->state.up);
	if (ImGuiToolKit::Button(u8"削除"))
	{
		map->RemoveAction(selectedAction);
		selectedAction.clear();
		return;
	}

	bool changed = false;
	int type = static_cast<int>(action->type);
	if (ImGuiToolKit::Combo(u8"タイプ", &type, JText(u8"ボタン\0ベクトル2\0")))
	{
		action->type = static_cast<ActionType>(type);
		changed = true;
	}
	if (action->type == ActionType::Vector2) changed |= DrawVector2Editor(*action);

	changed |= DrawBindings(system, JText(u8"バインディング"), action->bindings);
	if (action->type == ActionType::Vector2)
	{
		for (int direction = 0; direction < 4; ++direction)
			changed |= DrawBindings(system, directions[direction], action->vectorBinding.directions[direction]);
	}
	if (changed)
	{
		action->state = {};
		action->vector = {};
		action->physicalHeld = false;
	}
}

bool InputSystemGUI::DrawVector2Editor(InputAction& action)
{
	bool changed = false;
	auto& vector = action.vectorBinding;
	int x = static_cast<int>(vector.xAxis);
	int y = static_cast<int>(vector.yAxis);
	if (ImGuiToolKit::Combo(u8"X軸", &x, axes, 5))
	{
		vector.xAxis = static_cast<Axis>(x);
		changed = true;
	}
	if (ImGuiToolKit::Combo(u8"Y軸", &y, axes, 5))
	{
		vector.yAxis = static_cast<Axis>(y);
		changed = true;
	}
	changed |= ImGuiToolKit::SliderFloat(u8"デッドゾーン", &vector.deadZone, 0.01f, 1.0f);
	changed |= ImGuiToolKit::Checkbox(u8"正規化", &vector.normalize);
	changed |= ImGuiToolKit::Checkbox(u8"マウス移動量", &vector.mouseDelta);
	ImGuiToolKit::Text(u8"値: %.3f, %.3f", action.vector.x, action.vector.y);
	return changed;
}

bool InputSystemGUI::DrawBindingEditor(InputSystem& system, Binding& binding)
{
	bool changed = false;
	int device = static_cast<int>(binding.device);
	if (ImGuiToolKit::Combo(u8"デバイス", &device, devices, 4))
	{
		binding = {};
		binding.device = static_cast<Device>(device);
		binding.code = GamePad::BTN_A;
		if (binding.device == Device::Keyboard) binding.code = VK_SPACE;
		if (binding.device == Device::Mouse) binding.code = Mouse::BTN_LEFT;
		binding.axis = Axis::LeftX;
		changed = true;
	}
	if (binding.device == Device::Keyboard)
	{
		if (ImGuiToolKit::BeginCombo(u8"キー", GetKeyName(binding.code).c_str()))
		{
			for (int key = 1; key < 255; ++key)
			{
				ImGui::PushID(key);
				if (ImGui::Selectable(GetKeyName(key).c_str(), key == binding.code))
				{
					binding.code = key;
					changed = true;
				}
				ImGui::PopID();
			}
			ImGui::EndCombo();
		}
	}
	else if (binding.device == Device::Axis)
	{
		int axis = static_cast<int>(binding.axis);
		if (ImGuiToolKit::Combo(u8"軸", &axis, axes, 5))
		{
			binding.axis = static_cast<Axis>(axis);
			changed = true;
		}
		changed |= ImGuiToolKit::Checkbox(u8"マイナス方向", &binding.negative);
		changed |= ImGuiToolKit::SliderFloat(u8"しきい値", &binding.threshold, 0.0f, 1.0f);
	}
	else
	{
		const char* mouseButtons[] = {JText(u8"左"), JText(u8"中央"), JText(u8"右")};
		const bool isMouse = binding.device == Device::Mouse;
		int button = -1;
		const int buttonCount = isMouse ? 3 : 16;
		for (int i = 0; i < buttonCount; ++i)
		{
			if (binding.code == (1 << i)) button = i;
		}
		if (ImGuiToolKit::Combo(u8"ボタン", &button, isMouse ? mouseButtons : padButtons, buttonCount))
		{
			binding.code = 1 << button;
			changed = true;
		}
	}
	if (binding.device != Device::Axis) changed |= ImGuiToolKit::Checkbox(u8"押した瞬間だけ有効", &binding.downOnly);
	const auto state = system.ReadBindings(std::span<const Binding>(&binding, 1));
	ImGuiToolKit::TextDisabled(u8"バインディングの押下状態: %d", state.held);
	return changed;
}
}
