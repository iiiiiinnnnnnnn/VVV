#include <ImGuiToolKit/ImGuiToolKit.h>
#include <InputKit/InputSystemGUI.h>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <algorithm>

namespace InputKit
{
static const char* devices[] = {"Keyboard", "Mouse", "GamePad", "Axis"};
static const char* axes[] = {"None", "Left Stick X", "Left Stick Y", "Right Stick X", "Right Stick Y"};
static const char* directions[] = {"Left", "Right", "Backward", "Forward"};
static const char* padButtons[] = {"D-Pad Up", "D-Pad Right", "D-Pad Down", "D-Pad Left",
	"A", "B", "X", "Y", "Start", "Back", "Left Thumb", "Right Thumb",
	"Left Shoulder", "Right Shoulder", "Left Trigger", "Right Trigger"};

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
	case Device::Keyboard: return GetKeyName(binding.code) + " [Keyboard]";
	case Device::Mouse:
		if (binding.code == Mouse::BTN_LEFT) return "Left [Mouse]";
		if (binding.code == Mouse::BTN_MIDDLE) return "Middle [Mouse]";
		return "Right [Mouse]";
	case Device::GamePad:
		for (int i = 0; i < 16; ++i)
		{
			if (binding.code == (1 << i)) return std::string(padButtons[i]) + " [GamePad]";
		}
		return std::string("Button mask ") + std::to_string(binding.code) + " [GamePad]";
	case Device::Axis:
		return std::string(axes[std::clamp(static_cast<int>(binding.axis), 0, 4)]) + (binding.negative ? " - [Axis]" : " + [Axis]");
	}
	return std::string("Unknown binding");
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
		ImGui::TableSetupColumn("Action Maps", ImGuiTableColumnFlags_WidthStretch, 0.7f);
		ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableSetupColumn("Details", ImGuiTableColumnFlags_WidthStretch, 1.3f);
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
	const std::string preview = files[selected].path.empty() ? "(Unsaved)" : files[selected].path.filename().generic_string();
	if (ImGui::BeginCombo("Input file", preview.c_str()))
	{
		for (size_t index = 0; index < files.size(); ++index)
		{
			ImGui::PushID(static_cast<int>(index));
			const std::string name = files[index].path.empty() ? "(Unsaved)" : files[index].path.generic_string();
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
	if (ImGui::Button("Save"))
	{
		if (!system.Save(filePathEdit)) ImGuiToolKit::Alert(system.GetLastError(), "Input file error");
	}
	ImGui::SameLine();
	if (ImGui::Button("Load"))
	{
		if (system.Load(filePathEdit))
		{
			selectedMap.clear();
			selectedAction.clear();
			editingTarget.clear();
		}
		else ImGuiToolKit::Alert(system.GetLastError(), "Input file error");
	}

}

void InputSystemGUI::DrawMaps(InputSystem& system)
{
	auto& actions = system.GetActions();
	ImGui::SeparatorText("Action Maps");
	if (ImGui::Button("+ Map"))
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
	ImGui::SeparatorText("Actions");
	auto* map = actions.FindMap(selectedMap);
	if (!map) return;

	if (ImGui::Button("+ Action"))
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
	if (ImGui::Button("+ Binding"))
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
		if (ImGui::Button("Delete")) removeIndex = index;
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
	ImGui::SeparatorText("Details");
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
	if (ImGui::InputText("Name", &editName, ImGuiInputTextFlags_EnterReturnsTrue))
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
	if (nameError) ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Use a unique nonempty name without '/'.");
	if (!action)
	{
		if (ImGui::Button("Delete"))
		{
			actions.RemoveMap(selectedMap);
			selectedMap.clear();
			selectedAction.clear();
		}
		return;
	}

	ImGui::TextWrapped("%s / %s", selectedMap.c_str(), selectedAction.c_str());
	ImGui::TextDisabled("Held %d  Down %d  Up %d", action->state.held, action->state.down, action->state.up);
	if (ImGui::Button("Delete"))
	{
		map->RemoveAction(selectedAction);
		selectedAction.clear();
		return;
	}

	bool changed = false;
	int type = static_cast<int>(action->type);
	if (ImGui::Combo("Type", &type, "Button\0Vector2\0"))
	{
		action->type = static_cast<ActionType>(type);
		changed = true;
	}
	if (action->type == ActionType::Vector2) changed |= DrawVector2Editor(*action);

	changed |= DrawBindings(system, "Bindings", action->bindings);
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
	if (ImGui::Combo("X Axis", &x, axes, 5))
	{
		vector.xAxis = static_cast<Axis>(x);
		changed = true;
	}
	if (ImGui::Combo("Y Axis", &y, axes, 5))
	{
		vector.yAxis = static_cast<Axis>(y);
		changed = true;
	}
	changed |= ImGui::SliderFloat("Dead Zone", &vector.deadZone, 0.01f, 1.0f);
	changed |= ImGui::Checkbox("Normalize", &vector.normalize);
	changed |= ImGui::Checkbox("Mouse Delta", &vector.mouseDelta);
	ImGui::Text("Value: %.3f, %.3f", action.vector.x, action.vector.y);
	return changed;
}

bool InputSystemGUI::DrawBindingEditor(InputSystem& system, Binding& binding)
{
	bool changed = false;
	int device = static_cast<int>(binding.device);
	if (ImGui::Combo("Device", &device, devices, 4))
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
		if (ImGui::BeginCombo("Key", GetKeyName(binding.code).c_str()))
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
		if (ImGui::Combo("Axis", &axis, axes, 5))
		{
			binding.axis = static_cast<Axis>(axis);
			changed = true;
		}
		changed |= ImGui::Checkbox("Negative", &binding.negative);
		changed |= ImGui::SliderFloat("Threshold", &binding.threshold, 0.0f, 1.0f);
	}
	else
	{
		const char* mouseButtons[] = {"Left", "Middle", "Right"};
		const bool isMouse = binding.device == Device::Mouse;
		int button = -1;
		const int buttonCount = isMouse ? 3 : 16;
		for (int i = 0; i < buttonCount; ++i)
		{
			if (binding.code == (1 << i)) button = i;
		}
		if (ImGui::Combo("Button", &button, isMouse ? mouseButtons : padButtons, buttonCount))
		{
			binding.code = 1 << button;
			changed = true;
		}
	}
	if (binding.device != Device::Axis) changed |= ImGui::Checkbox("Pulse on Down", &binding.downOnly);
	const auto state = system.ReadBindings(std::span<const Binding>(&binding, 1));
	ImGui::TextDisabled("Binding held: %d", state.held);
	return changed;
}
}
