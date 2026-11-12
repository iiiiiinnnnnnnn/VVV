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
	throw std::runtime_error("不明な入力タイプ: " + text);
}

static void ValidateInputName(const std::string& name)
{
	if (name.empty() || name.size() > 255 || name.find('/') != std::string::npos)
		throw std::runtime_error("入力名は空欄を避け、255バイト以内で「/」を含めずに指定してください。");
}

static void ValidateInputBinding(const Binding& binding)
{
	const int device = static_cast<int>(binding.device);
	const int axis = static_cast<int>(binding.axis);
	if (device < 0 || device >= 4 || axis < 0 || axis >= 5) throw std::runtime_error("入力タイプが不正です。");
	if (!std::isfinite(binding.threshold) || binding.threshold < 0 || binding.threshold > 1)
		throw std::runtime_error("しきい値は0～1の範囲で指定してください。");
	switch (binding.device)
	{
	case Device::Keyboard:
		if (binding.code < 1 || binding.code > 254) throw std::runtime_error("キーボードのキーが不正です。");
		break;
	case Device::Mouse:
		if (binding.code != Mouse::BTN_LEFT && binding.code != Mouse::BTN_MIDDLE && binding.code != Mouse::BTN_RIGHT)
			throw std::runtime_error("マウスボタンが不正です。");
		break;
	case Device::GamePad:
		if (binding.code <= 0 || binding.code >= (1 << 16)) throw std::runtime_error("ゲームパッドのボタンが不正です。");
		break;
	case Device::Axis:
		if (binding.axis == Axis::None) throw std::runtime_error("軸のバインディングには軸の指定が必要です。");
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
	if (!value.is_array() || value.size() > 256) throw std::runtime_error("バインディングの配列が不正です。");
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
	if (bindings.size() > 256) throw std::runtime_error("バインディングの数が上限を超えています。");
	InputJson result = InputJson::array();
	for (const auto& binding : bindings) result.push_back(WriteInputBinding(binding));
	return result;
}

static InputAction ReadInputAction(const InputJson& definition)
{
	const auto type = definition.at("type").get<std::string>();
	if (type != "Button" && type != "Vector2") throw std::runtime_error("不明なアクションタイプです。");

	InputAction action;
	action.type = type == "Button" ? ActionType::Button : ActionType::Vector2;
	action.bindings = ReadInputBindings(definition.at("bindings"));
	const auto& vector = definition.at("vector2");
	auto& binding = action.vectorBinding;
	const auto& directions = vector.at("directions");

	if (!directions.is_array() || directions.size() != 4) throw std::runtime_error("ベクトル2には4方向の指定が必要です。");
	for (size_t i = 0; i < 4; ++i) binding.directions[i] = ReadInputBindings(directions[i]);
	binding.xAxis = static_cast<Axis>(ReadInputEnum(vector.at("xAxis"), AxisNames, 5));
	binding.yAxis = static_cast<Axis>(ReadInputEnum(vector.at("yAxis"), AxisNames, 5));
	binding.deadZone = vector.at("deadZone").get<float>();
	binding.normalize = vector.at("normalize").get<bool>();
	binding.mouseDelta = vector.at("mouseDelta").get<bool>();

	if (!std::isfinite(binding.deadZone) || binding.deadZone < 0 || binding.deadZone > 1)
		throw std::runtime_error("デッドゾーンは0～1の範囲で指定してください。");

	return action;
}

static InputJson WriteInputAction(const InputAction& action)
{
	if (action.type != ActionType::Button && action.type != ActionType::Vector2) throw std::runtime_error("アクションタイプが不正です。");
	const auto& vector = action.vectorBinding;
	const int x = static_cast<int>(vector.xAxis);
	const int y = static_cast<int>(vector.yAxis);

	if (x < 0 || x >= 5 || y < 0 || y >= 5) throw std::runtime_error("ベクトル2の軸が不正です。");
	if (!std::isfinite(vector.deadZone) || vector.deadZone < 0 || vector.deadZone > 1)
		throw std::runtime_error("デッドゾーンは0～1の範囲で指定してください。");

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
		if (absolute.extension() != ".vis") throw std::runtime_error("入力ファイルの拡張子は.visにしてください。");
		if (std::filesystem::file_size(absolute) > 16 * 1024 * 1024) throw std::runtime_error("入力ファイルのサイズが上限を超えています。");
		std::ifstream input(absolute, std::ios::binary);
		if (!input) throw std::runtime_error("入力ファイルを開けません: " + absolute.string());
		const auto root = InputJson::parse(input);
		if (root.at("version").get<int>() != 1) throw std::runtime_error("未対応の入力ファイルバージョンです。");
		const auto& maps = root.at("maps");
		if (!maps.is_object() || maps.size() > 256) throw std::runtime_error("アクションマップが不正です。");
		InputActions loaded;
		for (const auto& [mapName, mapData] : maps.items())
		{
			ValidateInputName(mapName);
			auto& map = loaded.RegisterMap(mapName);
			const auto& definitions = mapData.at("actions");
			if (!definitions.is_object() || definitions.size() > 4096) throw std::runtime_error("アクションの定義が不正です。");
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
					throw std::runtime_error("アクションマップ名が重複しています '" + mapName + "': " + absolute.string());
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
		lastError = ".visファイルのパスが指定されていません。";
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
		if (absolute.extension() != ".vis") throw std::runtime_error("入力ファイルの拡張子は.visにしてください。");
		for (size_t index = 0; index < files.size(); ++index)
		{
			if (index != selectedFile && files[index].path == absolute)
				throw std::runtime_error("このパスは別の読み込み済み入力ファイルで使用されています。");
		}
		const auto& actions = GetActions();
		for (const auto& [mapName, map] : actions.GetMaps())
		{
			for (size_t index = 0; index < files.size(); ++index)
			{
				if (index != selectedFile && files[index].actions.FindMap(mapName))
					throw std::runtime_error("アクションマップ名が重複しています: " + mapName);
			}
		}
		InputJson root;
		root["version"] = 1;
		root["maps"] = InputJson::object();
		if (actions.GetMaps().size() > 256) throw std::runtime_error("アクションマップの数が上限を超えています。");
		for (const auto& [mapName, map] : actions.GetMaps())
		{
			ValidateInputName(mapName);
			if (map.GetActions().size() > 4096) throw std::runtime_error("アクションの数が上限を超えています。");
			auto definitions = InputJson::object();
			for (const auto& [name, action] : map.GetActions())
			{
				ValidateInputName(name);
				definitions[name] = WriteInputAction(action);
			}
			root["maps"][mapName]["actions"] = definitions;
		}
		const auto contents = root.dump(2) + '\n';
		if (contents.size() > 16 * 1024 * 1024) throw std::runtime_error("入力ファイルのサイズが上限を超えています。");
		std::filesystem::create_directories(absolute.parent_path());
		// 書込みが成功してから置き換え、既存のファイルを保護する。
		temporary = absolute;
		temporary += ".tmp." + std::to_string(GetCurrentProcessId());
		{
			std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
			if (!output) throw std::runtime_error("入力ファイル保存用の一時ファイルを作成できません。");
			output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
			output.flush();
			if (!output) throw std::runtime_error("入力ファイルを書き込めません。");
		}
		if (!MoveFileExW(temporary.c_str(), absolute.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
			throw std::runtime_error("入力ファイルを置き換えられません。Windowsエラー: " + std::to_string(::GetLastError()));
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
