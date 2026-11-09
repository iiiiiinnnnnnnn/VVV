#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <utility>

namespace InputKit
{
enum class Device { Keyboard, Mouse, GamePad, Axis };
enum class Axis { None, LeftX, LeftY, RightX, RightY };
enum class ActionType { Button, Vector2 };

struct Binding
{
	Device device = Device::Keyboard;
	int code = 'W';
	bool downOnly = false;
	Axis axis = Axis::None;
	float threshold = 0.35f;
	bool negative = false;
};

struct ButtonState
{
	bool held = false;
	bool down = false;
	bool up = false;
};

struct Vector2
{
	float x = 0.0f;
	float y = 0.0f;
};

struct Vector2Binding
{
	// Left, Right, Backward, Forward.
	std::array<std::vector<Binding>, 4> directions;
	Axis xAxis = Axis::LeftX;
	Axis yAxis = Axis::LeftY;
	float deadZone = 0.1f;
	bool normalize = true;
	bool mouseDelta = false;
};

struct InputAction
{
	// GUIから個別バインドを編集する。
	bool AddBinding(const Binding& binding)
	{
		bindings.push_back(binding);
		return true;
	}

	bool SetBinding(size_t index, const Binding& binding)
	{
		if (index >= bindings.size()) return false;
		bindings[index] = binding;
		return true;
	}

	bool RemoveBinding(size_t index)
	{
		if (index >= bindings.size()) return false;
		bindings.erase(bindings.begin() + index);
		return true;
	}

	bool SetVector2Binding(const Vector2Binding& binding)
	{
		vectorBinding = binding;
		return true;
	}

	ActionType type = ActionType::Button;
	std::vector<Binding> bindings;
	Vector2Binding vectorBinding;
	ButtonState state;
	Vector2 vector;
	bool physicalHeld = false;
};

class InputActionMap
{
public:
	InputAction& RegisterAction(const std::string& name, ActionType type = ActionType::Button)
	{
		auto& action = actions[name];
		action.type = type;
		return action;
	}

	InputAction* FindAction(std::string_view name)
	{
		const auto found = actions.find(std::string(name));
		return found != actions.end() ? &found->second : nullptr;
	}

	const InputAction* FindAction(std::string_view name) const
	{
		const auto found = actions.find(std::string(name));
		return found != actions.end() ? &found->second : nullptr;
	}

	void RemoveAction(std::string_view name) { actions.erase(std::string(name)); }
	// パス区切りを含む名前や、同名への上書きは許可しない。
	bool RenameAction(std::string_view oldName, std::string_view newName)
	{
		if (newName.empty() || newName.find('/') != std::string_view::npos) return false;
		if (oldName == newName) return FindAction(oldName);
		if (FindAction(newName)) return false;
		auto* oldAction = FindAction(oldName);
		if (!oldAction) return false;
		InputAction action = std::move(*oldAction);
		actions.erase(std::string(oldName));
		actions[std::string(newName)] = std::move(action);
		return true;
	}

	const auto& GetActions() const { return actions; }

private:
	friend class InputSystem;
	std::unordered_map<std::string, InputAction> actions;
};

class InputActions
{
public:
	InputActionMap& RegisterMap(const std::string& name) { return maps[name]; }
	// 登録済みのマップを削除・改名する。
	bool RemoveMap(std::string_view name) { return maps.erase(std::string(name)) != 0; }
	bool RenameMap(std::string_view oldName, std::string_view newName)
	{
		if (newName.empty() || newName.find('/') != std::string_view::npos) return false;
		if (oldName == newName) return FindMap(oldName);
		if (FindMap(newName)) return false;
		auto* oldMap = FindMap(oldName);
		if (!oldMap) return false;
		InputActionMap map = std::move(*oldMap);
		maps.erase(std::string(oldName));
		maps[std::string(newName)] = std::move(map);
		return true;
	}

	InputActionMap* FindMap(std::string_view name)
	{
		const auto found = maps.find(std::string(name));
		return found != maps.end() ? &found->second : nullptr;
	}

	const InputActionMap* FindMap(std::string_view name) const
	{
		const auto found = maps.find(std::string(name));
		return found != maps.end() ? &found->second : nullptr;
	}

	const auto& GetMaps() const { return maps; }

private:
	friend class InputSystem;
	std::unordered_map<std::string, InputActionMap> maps;
};
}
