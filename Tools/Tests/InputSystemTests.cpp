#include <InputKit/InputSystem.h>
#include <nlohmann/json.hpp>
#include <cassert>
#include <fstream>
#include <iostream>

static void WriteInputFile(const std::filesystem::path& path, const nlohmann::json& data)
{
	std::filesystem::create_directories(path.parent_path());
	std::ofstream file(path);
	file << data.dump(2);
}

int main()
{
	using InputKit::InputSystem;
	const auto root = std::filesystem::absolute("Obj/InputSystemFixtures/Singleton");
	const auto directory = root / "Input";
	const auto playerPath = directory / "PlayerInput.vis";
	const auto menuPath = directory / "UI/Menu.vis";
	const auto original = nlohmann::json::parse(std::ifstream("Resources/Input/PlayerInput.vis"));
	WriteInputFile(playerPath, original);
	nlohmann::json menu;
	menu["version"] = 1;
	menu["maps"]["Menu"]["actions"]["Confirm"] = original["maps"]["PlayerInput"]["actions"]["Attack"];
	WriteInputFile(menuPath, menu);

	auto& input = InputSystem::Instance();
	assert(&input == &InputSystem::Instance());
	input.Initialize(nullptr, directory);
	assert(input.GetFiles().size() == 2);
	assert(input.FindAction("PlayerInput/Attack"));
	assert(input.FindAction("Menu/Confirm"));
	assert(input.FindAction("Move")->vectorBinding.directions[0][0].code == 'A');

	// GUIの選択を切り替えても、全ファイルの入力を更新する。
	for (size_t index = 0; index < input.GetFiles().size(); ++index)
	{
		if (input.GetFiles()[index].path == menuPath) input.SelectFile(index);
	}
	assert(input.GetActions().FindMap("Menu"));
	input.Update(true, 0, 0);
	input.Update(true, 0, 0);
	input.FindAction("PlayerInput/Attack")->state = {true, true, false};
	input.FindAction("Menu/Confirm")->state = {true, false, true};
	assert(input.IsDown("PlayerInput/Attack"));
	assert(input.IsPressed("Menu/Confirm") && input.IsUp("Menu/Confirm"));

	// 選択したファイルだけ保存する。
	input.FindAction("Menu/Confirm")->bindings = {{InputKit::Device::Keyboard, 'J'}};
	assert(input.Save());
	const auto savedMenu = nlohmann::json::parse(std::ifstream(menuPath));
	assert(savedMenu["maps"].size() == 1 && savedMenu["maps"].contains("Menu"));
	assert(nlohmann::json::parse(std::ifstream(playerPath)) == original);
	assert(input.Load(playerPath));
	assert(input.FindAction("Menu/Confirm")->bindings[0].code == 'J');
	assert(input.GetFiles().size() == 2);

	// 別ファイルと同名のMap、不正なJSONは現在の定義を壊さない。
	const auto duplicate = root / "Duplicate.vis";
	WriteInputFile(duplicate, original);
	assert(!input.Load(duplicate));
	assert(input.GetFiles().size() == 2 && input.GetFilePath() == playerPath);
	const auto malformed = root / "Malformed.vis";
	{ std::ofstream file(malformed); file << "{\"maps\":[] }"; }
	assert(!input.Load(malformed));
	assert(input.FindAction("Menu/Confirm") && input.FindAction("PlayerInput/Attack"));
	assert(!input.RenameMap("PlayerInput", "Menu"));

	// 保存失敗時には、元のファイルを残す。
	const int originalKey = input.FindAction("Attack")->bindings[0].code;
	input.FindAction("Attack")->bindings[0].code = -1;
	assert(!input.Save());
	assert(nlohmann::json::parse(std::ifstream(playerPath)) == original);
	input.FindAction("Attack")->bindings[0].code = originalKey;

	std::cout << "InputSystem singleton/multiple-file tests passed.\n";
}
