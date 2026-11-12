#include <InputKit/InputSystem.h>
#include <utility>
#include <cmath>

InputKit::InputSystem& InputKit::InputSystem::Instance()
{
	static InputSystem instance;
	return instance;
}

void InputKit::InputSystem::Initialize(HWND hwnd, const std::filesystem::path& directory)
{
	window = hwnd;
	mouse = Mouse(hwnd);
	gamePad = {};
	keys.fill(false);
	previousKeys.fill(false);
	enabled = false;
	suppressionFrames = 0;
	LoadAll(directory);
}

void InputKit::InputSystem::Update()
{
	bool acceptsInput = window && GetForegroundWindow() == window;
	if (focusCheck && !focusCheck(true)) acceptsInput = false;
	Update(acceptsInput, viewportWidth, viewportHeight);
}

bool InputKit::InputSystem::IsFocusedWindow(bool ignoreUI)
{
	const auto& input = Instance();
	if (!input.window || GetForegroundWindow() != input.window) return false;
	if (input.focusCheck && !input.focusCheck(ignoreUI)) return false;
	return true;
}

bool InputKit::InputSystem::CanReadActions() const
{
	if (!enabled || suppressionFrames > 0) return false;
	if (focusCheck && !focusCheck(false)) return false;
	return true;
}

bool InputKit::InputSystem::IsPressed(const std::string& path, bool physicalInput) const
{
	if (!CanReadActions()) return false;
	const auto* action = FindAction(path);
	if (!action) return false;
	if (physicalInput) return ReadBindings(action->bindings, true).held;
	return action->state.held;
}

bool InputKit::InputSystem::IsDown(const std::string& path, bool ignoreUI) const
{
	if (!enabled) return false;
	if (!ignoreUI && !CanReadActions()) return false;
	const auto* action = FindAction(path);
	return action && action->state.down;
}

bool InputKit::InputSystem::IsUp(const std::string& path) const
{
	if (!CanReadActions()) return false;
	const auto* action = FindAction(path);
	return action && action->state.up;
}

bool InputKit::InputSystem::IsAnyDown(std::span<const int> excludedKeys, bool ignoreUI) const
{
	if (!enabled || (!ignoreUI && !CanReadActions())) return false;
	if (mouse.GetButtonDown() || gamePad.GetButtonDown()) return true;
	for (int key = 0; key < static_cast<int>(keys.size()); ++key)
	{
		if (!keys[key] || previousKeys[key]) continue;
		if (std::find(excludedKeys.begin(), excludedKeys.end(), key) == excludedKeys.end()) return true;
	}
	return false;
}

void InputKit::InputSystem::Update(bool acceptsInput, UINT _viewportWidth, UINT _viewportHeight)
{
	if (suppressionFrames > 0) --suppressionFrames;
	previousKeys = keys;
	enabled = acceptsInput;
	for (int key = 0; key < static_cast<int>(keys.size()); ++key)
		keys[key] = enabled && (GetAsyncKeyState(key) & 0x8000) != 0;
	gamePad.Update(enabled);
	mouse.Update(_viewportWidth, _viewportHeight);

	for (auto& file : files)
	{
		for (auto& [mapName, map] : file.actions.maps)
		{
			for (auto& [name, action] : map.actions)
			{
				const auto physical = ReadBindings(action.bindings, true);
				const auto effective = ReadBindings(action.bindings);
				action.state.down = physical.held && !action.physicalHeld;
				action.state.up = !physical.held && action.physicalHeld;
				action.state.held = effective.held;
				action.physicalHeld = physical.held;
				if (action.type == ActionType::Vector2) action.vector = ReadVector2(action.vectorBinding);
			}
		}
	}
}

InputKit::InputActionMap* InputKit::InputSystem::FindActionMap(std::string_view name)
{
	for (auto& file : files)
	{
		if (auto* map = file.actions.FindMap(name)) return map;
	}
	return nullptr;
}

const InputKit::InputActionMap* InputKit::InputSystem::FindActionMap(std::string_view name) const
{
	for (const auto& file : files)
	{
		if (const auto* map = file.actions.FindMap(name)) return map;
	}
	return nullptr;
}

bool InputKit::InputSystem::RenameMap(std::string_view oldName, std::string_view newName)
{
	if (oldName != newName && FindActionMap(newName)) return false;
	return GetActions().RenameMap(oldName, newName);
}

void InputKit::InputSystem::Bind(const std::string& path, Binding binding)
{
	const auto separator = path.find('/');
	const auto mapName = separator == std::string::npos ? "Global" : path.substr(0, separator);
	const auto name = separator == std::string::npos ? path : path.substr(separator + 1);
	auto* map = FindActionMap(mapName);
	if (!map) map = &GetActions().RegisterMap(mapName);
	map->RegisterAction(name).bindings.push_back(binding);
}

void InputKit::InputSystem::SetBindings(const std::string& path, std::vector<Binding> bindings)
{
	const auto separator = path.find('/');
	const auto mapName = separator == std::string::npos ? "Global" : path.substr(0, separator);
	const auto name = separator == std::string::npos ? path : path.substr(separator + 1);
	auto* map = FindActionMap(mapName);
	if (!map) map = &GetActions().RegisterMap(mapName);
	auto& action = map->RegisterAction(name);
	action.bindings = std::move(bindings);
	action.state = {};
	action.physicalHeld = false;
}

InputKit::InputAction* InputKit::InputSystem::FindAction(std::string_view path)
{
	const auto separator = path.find('/');
	if (separator != std::string_view::npos)
	{
		auto* map = FindActionMap(path.substr(0, separator));
		return map ? map->FindAction(path.substr(separator + 1)) : nullptr;
	}

	InputAction* match = nullptr;
	for (auto& file : files)
	{
		for (auto& [name, map] : file.actions.maps)
		{
			auto* action = map.FindAction(path);
			if (!action) continue;
			if (match) return nullptr; // 同名の場合は Map/Action で指定する。
			match = action;
		}
	}
	return match;
}

const InputKit::InputAction* InputKit::InputSystem::FindAction(std::string_view path) const
{
	const auto separator = path.find('/');
	if (separator != std::string_view::npos)
	{
		const auto* map = FindActionMap(path.substr(0, separator));
		return map ? map->FindAction(path.substr(separator + 1)) : nullptr;
	}

	const InputAction* match = nullptr;
	for (const auto& file : files)
	{
		for (const auto& [name, map] : file.actions.maps)
		{
			const auto* action = map.FindAction(path);
			if (!action) continue;
			if (match) return nullptr;
			match = action;
		}
	}
	return match;
}

void InputKit::InputSystem::RemoveAction(const std::string& path)
{
	const auto separator = path.find('/');
	const auto mapName = separator == std::string::npos ? "Global" : path.substr(0, separator);
	const auto name = separator == std::string::npos ? path : path.substr(separator + 1);
	if (auto* map = FindActionMap(mapName)) map->RemoveAction(name);
}

void InputKit::InputSystem::LoadAll(const std::filesystem::path& directory)
{
	// サブフォルダー内含む検索
	for (const auto& entry : std::filesystem::recursive_directory_iterator(directory))
	{
		if (!entry.is_regular_file()) continue;
		if (entry.path().extension() != ".vis") continue;

		Load(entry.path());
	}
}

InputKit::Vector2 InputKit::InputSystem::ReadVector2(std::string_view path) const
{
	if (!CanReadActions()) return {};
	const auto* action = FindAction(path);
	if (!action || action->type != ActionType::Vector2) return {};
	return action->vector;
}

InputKit::Vector2 InputKit::InputSystem::ReadVector2(const Vector2Binding& binding) const
{
	Vector2 value{ReadAxis(binding.xAxis), ReadAxis(binding.yAxis)};
	if (std::abs(value.x) <= binding.deadZone && std::abs(value.y) <= binding.deadZone)
	{
		if (ReadBindings(binding.directions[3]).held) value.y = 1.0f;
		if (ReadBindings(binding.directions[2]).held) value.y = -1.0f;
		if (ReadBindings(binding.directions[0]).held) value.x = -1.0f;
		if (ReadBindings(binding.directions[1]).held) value.x = 1.0f;
		const float length = std::sqrt(value.x * value.x + value.y * value.y);
		if (binding.normalize && length > 1.0f)
		{
			value.x /= length;
			value.y /= length;
		}
	}
	if (enabled && binding.mouseDelta)
	{
		value.x += static_cast<float>(mouse.GetAxisX());
		value.y += static_cast<float>(mouse.GetAxisY());
	}
	return value;
}

InputKit::ButtonState InputKit::InputSystem::ReadBindings(std::span<const Binding> bindings,
	bool physicalInput) const
{
	ButtonState result;
	if (!enabled) return result;
	for (const auto& binding : bindings)
	{
		bool held = false;
		bool down = false;
		bool up = false;
		switch (binding.device)
		{
		case Device::Keyboard:
			if (binding.code < 0 || binding.code >= static_cast<int>(keys.size())) continue;
			held = keys[binding.code];
			down = held && !previousKeys[binding.code];
			up = !held && previousKeys[binding.code];
			break;
		case Device::Mouse:
			held = (mouse.GetButton() & binding.code) != 0;
			down = (mouse.GetButtonDown() & binding.code) != 0;
			up = (mouse.GetButtonUp() & binding.code) != 0;
			break;
		case Device::Axis:
		{
			float value = ReadAxis(binding.axis);
			if (binding.negative) value = -value;
			held = value > binding.threshold;
			break;
		}
		case Device::GamePad:
			held = (gamePad.GetButton() & binding.code) != 0;
			down = (gamePad.GetButtonDown() & binding.code) != 0;
			up = (gamePad.GetButtonUp() & binding.code) != 0;
			break;
		}
		// Pulse設定でも、押下・解放の判定には実際の押下状態を使う。
		if (binding.downOnly && !physicalInput) held = down;
		result.held = result.held || held;
		result.down = result.down || down;
		result.up = result.up || up;
	}
	return result;
}

float InputKit::InputSystem::ReadAxis(Axis axis) const
{
	if (!enabled) return 0.0f;
	switch (axis)
	{
	case Axis::LeftX: return gamePad.GetAxisLX();
	case Axis::LeftY: return gamePad.GetAxisLY();
	case Axis::RightX: return gamePad.GetAxisRX();
	case Axis::RightY: return gamePad.GetAxisRY();
	default: return 0.0f;
	}
}

