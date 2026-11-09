#include <InputKit/InputSystem.h>
#include <nlohmann/json.hpp>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace InputKit
{
using InputJson = nlohmann::json;
static constexpr const char* DeviceNames[] = {"Keyboard", "Mouse", "GamePad", "Axis"};
static constexpr const char* AxisNames[] = {"None", "LeftX", "LeftY", "RightX", "RightY"};

static int ReadInputEnum(const InputJson& value, const char* const* names, int count)
{
	const auto text = value.get<std::string>();
	for (int i = 0; i < count; ++i)
	{
		if (text == names[i]) return i;
	}
	throw std::runtime_error("Unknown input enum: " + text);
}

static void ValidateInputName(const std::string& name)
{
	if (name.empty() || name.size() > 255 || name.find('/') != std::string::npos)
		throw std::runtime_error("Input names must be nonempty, at most 255 bytes, and contain no '/'.");
}

static void ValidateInputBinding(const Binding& binding)
{
	const int device = static_cast<int>(binding.device);
	const int axis = static_cast<int>(binding.axis);
	if (device < 0 || device >= 4 || axis < 0 || axis >= 5) throw std::runtime_error("Invalid input enum.");
	if (!std::isfinite(binding.threshold) || binding.threshold < 0 || binding.threshold > 1)
		throw std::runtime_error("Input threshold must be within 0..1.");
	switch (binding.device)
	{
	case Device::Keyboard:
		if (binding.code < 1 || binding.code > 254) throw std::runtime_error("Invalid keyboard key.");
		break;
	case Device::Mouse:
		if (binding.code != Mouse::BTN_LEFT && binding.code != Mouse::BTN_MIDDLE && binding.code != Mouse::BTN_RIGHT)
			throw std::runtime_error("Invalid mouse button.");
		break;
	case Device::GamePad:
		if (binding.code <= 0 || binding.code >= (1 << 16)) throw std::runtime_error("Invalid gamepad button.");
		break;
	case Device::Axis:
		if (binding.axis == Axis::None) throw std::runtime_error("An axis binding needs an axis.");
		break;
	}
}

static Binding ReadInputBinding(const InputJson& value)
{
	Binding result;
	result.device = static_cast<Device>(ReadInputEnum(value.at("device"), DeviceNames, 4));
	result.code = value.at("code").get<int>();
	result.downOnly = value.at("downOnly").get<bool>();
	result.axis = static_cast<Axis>(ReadInputEnum(value.at("axis"), AxisNames, 5));
	result.threshold = value.at("threshold").get<float>();
	result.negative = value.at("negative").get<bool>();
	ValidateInputBinding(result);
	return result;
}

static std::vector<Binding> ReadInputBindings(const InputJson& value)
{
	if (!value.is_array() || value.size() > 256) throw std::runtime_error("Invalid binding array.");
	std::vector<Binding> result;
	for (const auto& binding : value) result.push_back(ReadInputBinding(binding));
	return result;
}

static InputJson WriteInputBinding(const Binding& binding)
{
	ValidateInputBinding(binding);
	const int device = static_cast<int>(binding.device);
	const int axis = static_cast<int>(binding.axis);
	InputJson result;
	result["device"] = DeviceNames[device];
	result["code"] = binding.code;
	result["downOnly"] = binding.downOnly;
	result["axis"] = AxisNames[axis];
	result["threshold"] = binding.threshold;
	result["negative"] = binding.negative;
	return result;
}

static InputJson WriteInputBindings(const std::vector<Binding>& bindings)
{
	if (bindings.size() > 256) throw std::runtime_error("Too many bindings.");
	InputJson result = InputJson::array();
	for (const auto& binding : bindings) result.push_back(WriteInputBinding(binding));
	return result;
}

static InputAction ReadInputAction(const InputJson& definition)
{
	const auto type = definition.at("type").get<std::string>();
	if (type != "Button" && type != "Vector2") throw std::runtime_error("Unknown action type.");

	InputAction action;
	action.type = type == "Button" ? ActionType::Button : ActionType::Vector2;
	action.bindings = ReadInputBindings(definition.at("bindings"));
	const auto& vector = definition.at("vector2");
	auto& binding = action.vectorBinding;
	const auto& directions = vector.at("directions");

	if (!directions.is_array() || directions.size() != 4) throw std::runtime_error("A Vector2 must have four directions.");
	for (size_t i = 0; i < 4; ++i) binding.directions[i] = ReadInputBindings(directions[i]);
	binding.xAxis = static_cast<Axis>(ReadInputEnum(vector.at("xAxis"), AxisNames, 5));
	binding.yAxis = static_cast<Axis>(ReadInputEnum(vector.at("yAxis"), AxisNames, 5));
	binding.deadZone = vector.at("deadZone").get<float>();
	binding.normalize = vector.at("normalize").get<bool>();
	binding.mouseDelta = vector.at("mouseDelta").get<bool>();

	if (!std::isfinite(binding.deadZone) || binding.deadZone < 0 || binding.deadZone > 1)
		throw std::runtime_error("Input dead zone must be within 0..1.");

	return action;
}

static InputJson WriteInputAction(const InputAction& action)
{
	if (action.type != ActionType::Button && action.type != ActionType::Vector2) throw std::runtime_error("Invalid action type.");
	const auto& vector = action.vectorBinding;
	const int x = static_cast<int>(vector.xAxis);
	const int y = static_cast<int>(vector.yAxis);

	if (x < 0 || x >= 5 || y < 0 || y >= 5) throw std::runtime_error("Invalid Vector2 axes.");
	if (!std::isfinite(vector.deadZone) || vector.deadZone < 0 || vector.deadZone > 1)
		throw std::runtime_error("Input dead zone must be within 0..1.");

	auto directions = InputJson::array();
	for (const auto& bindings : vector.directions) directions.push_back(WriteInputBindings(bindings));

	InputJson vectorData;
	vectorData["directions"] = directions;
	vectorData["xAxis"] = AxisNames[x];
	vectorData["yAxis"] = AxisNames[y];
	vectorData["deadZone"] = vector.deadZone;
	vectorData["normalize"] = vector.normalize;
	vectorData["mouseDelta"] = vector.mouseDelta;

	InputJson definition;
	definition["type"] = action.type == ActionType::Button ? "Button" : "Vector2";
	definition["bindings"] = WriteInputBindings(action.bindings);
	definition["vector2"] = vectorData;
	return definition;
}

bool InputSystem::Load(const std::filesystem::path& path)
{
	try
	{
		const auto absolute = std::filesystem::absolute(path).lexically_normal();
		if (absolute.extension() != ".vis") throw std::runtime_error("Input files must use .vis.");
		if (std::filesystem::file_size(absolute) > 16 * 1024 * 1024) throw std::runtime_error("Input file is too large.");
		std::ifstream input(absolute, std::ios::binary);
		if (!input) throw std::runtime_error("Cannot open input file: " + absolute.string());
		const auto root = InputJson::parse(input);
		if (root.at("version").get<int>() != 1) throw std::runtime_error("Unsupported input file version.");
		const auto& maps = root.at("maps");
		if (!maps.is_object() || maps.size() > 256) throw std::runtime_error("Invalid input maps.");
		InputActions loaded;
		for (const auto& [mapName, mapData] : maps.items())
		{
			ValidateInputName(mapName);
			auto& map = loaded.RegisterMap(mapName);
			const auto& definitions = mapData.at("actions");
			if (!definitions.is_object() || definitions.size() > 4096) throw std::runtime_error("Invalid action definitions.");
			for (const auto& [name, definition] : definitions.items())
			{
				ValidateInputName(name);
				map.RegisterAction(name) = ReadInputAction(definition);
			}
		}
		// 別ファイルと同じMap名は、どちらを使うか曖昧になるため拒否する。
		for (const auto& file : files)
		{
			if (file.path == absolute) continue;
			for (const auto& [mapName, map] : loaded.GetMaps())
			{
				if (file.actions.FindMap(mapName))
					throw std::runtime_error("Duplicate input map '" + mapName + "': " + absolute.string());
			}
		}

		size_t index = 0;
		while (index < files.size() && files[index].path != absolute) ++index;
		if (index == files.size())
		{
			if (files.size() == 1 && files[0].path.empty() && files[0].actions.GetMaps().empty()) index = 0;
			else files.push_back(File{});
		}
		// 読込が成功したファイルだけを置き換え、ほかの定義は残す。
		files[index].path = absolute;
		files[index].actions = std::move(loaded);
		selectedFile = index;
		keys.fill(false);
		previousKeys.fill(false);
		SuppressGameplayInput();
		lastError.clear();
		return true;
	}
	catch (const std::exception& error)
	{
		lastError = path.string() + ": " + error.what();
		return false;
	}
}

bool InputSystem::Save()
{
	if (GetFilePath().empty())
	{
		lastError = "No .vis file path is selected.";
		return false;
	}
	return Save(GetFilePath());
}

bool InputSystem::Save(const std::filesystem::path& path)
{
	std::filesystem::path temporary;
	try
	{
		const auto absolute = std::filesystem::absolute(path).lexically_normal();
		if (absolute.extension() != ".vis") throw std::runtime_error("Input files must use .vis.");
		for (size_t index = 0; index < files.size(); ++index)
		{
			if (index != selectedFile && files[index].path == absolute)
				throw std::runtime_error("Another loaded input file already uses this path.");
		}
		const auto& actions = GetActions();
		for (const auto& [mapName, map] : actions.GetMaps())
		{
			for (size_t index = 0; index < files.size(); ++index)
			{
				if (index != selectedFile && files[index].actions.FindMap(mapName))
					throw std::runtime_error("Duplicate input map: " + mapName);
			}
		}
		InputJson root;
		root["version"] = 1;
		root["maps"] = InputJson::object();
		if (actions.GetMaps().size() > 256) throw std::runtime_error("Too many input maps.");
		for (const auto& [mapName, map] : actions.GetMaps())
		{
			ValidateInputName(mapName);
			if (map.GetActions().size() > 4096) throw std::runtime_error("Too many input actions.");
			auto definitions = InputJson::object();
			for (const auto& [name, action] : map.GetActions())
			{
				ValidateInputName(name);
				definitions[name] = WriteInputAction(action);
			}
			root["maps"][mapName]["actions"] = definitions;
		}
		const auto contents = root.dump(2) + '\n';
		if (contents.size() > 16 * 1024 * 1024) throw std::runtime_error("Input file is too large.");
		std::filesystem::create_directories(absolute.parent_path());
		// 書込みが成功してから置き換え、既存のファイルを保護する。
		temporary = absolute;
		temporary += ".tmp." + std::to_string(GetCurrentProcessId());
		{
			std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
			if (!output) throw std::runtime_error("Cannot create temporary input file.");
			output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
			output.flush();
			if (!output) throw std::runtime_error("Cannot write input file.");
		}
		if (!MoveFileExW(temporary.c_str(), absolute.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
			throw std::runtime_error("Cannot replace input file. Windows error: " + std::to_string(::GetLastError()));
		files[selectedFile].path = absolute;
		lastError.clear();
		return true;
	}
	catch (const std::exception& error)
	{
		lastError = error.what();
		if (!temporary.empty())
		{
			std::error_code ignored;
			std::filesystem::remove(temporary, ignored);
		}
		return false;
	}
}
}
