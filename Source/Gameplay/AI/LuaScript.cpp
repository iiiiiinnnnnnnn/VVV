
#include "LuaScript.h"

#include <iostream>
#include <system_error>
#include <utility>

#include "Application/Time/GameTime.h"
#include "Resource/ResourceManager.h"
#include "Resource/ResourcePath.h"

LuaScript::LuaScript(Object* owner) : Component(owner)
{
	lua = std::make_unique<sol::state>();

	lua->open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);
}

LuaScript::~LuaScript()
{
	updateFunc = sol::protected_function{};
	startFunc = sol::protected_function{};
	environment = sol::environment{};
	lua.reset();
}

bool LuaScript::Load(const std::string& path)
{
	// Debug/Developmentで親階層にVEER.slnがあれば、プロジェクトのResourcesを使う。
	// 見つからない場合とReleaseは、実行ファイル側のキャッシュを使う。
	// 解決した絶対パスを保持し、読み込みと更新日時の監視を同じファイルへ向ける。
	scriptPath = std::filesystem::absolute(ResourceManager::ResolveSourcePath(
		ResourcePath::Path(ResourceManager::Instance().ResolveResourcePath(path, ResourceManager::ResourceType::AI))))
		.lexically_normal().string();
	hasWriteTime = false;
	return Reload();
}

bool LuaScript::CreateEnvironment(sol::environment& newEnvironment,
	sol::protected_function& newStart, sol::protected_function& newUpdate)
{
	sol::load_result chunk = lua->load_file(scriptPath);

	if (!chunk.valid())
	{
		sol::error error = chunk;
		std::cerr << "[Lua] " << error.what() << '\n';
		return false;
	}

	newEnvironment = sol::environment(*lua, sol::create, lua->globals());

	BindEnvironment(newEnvironment);

	sol::protected_function script = chunk;
	sol::set_environment(newEnvironment, script);

	sol::protected_function_result result = script();

	if (!result.valid())
	{
		sol::error error = result;
		std::cerr << "[Lua] " << error.what() << '\n';
		return false;
	}

	sol::object start = newEnvironment["Start"];
	sol::object update = newEnvironment["Update"];

	if (start.get_type() == sol::type::function) newStart = start.as<sol::protected_function>();

	if (update.get_type() == sol::type::function) newUpdate = update.as<sol::protected_function>();

	return true;
}

bool LuaScript::Reload()
{
	if (scriptPath.empty()) return false;

	sol::environment newEnvironment;
	sol::protected_function newStart;
	sol::protected_function newUpdate;

	if (!CreateEnvironment(newEnvironment, newStart, newUpdate))
	{
		return false;
	}

	if (started) CallScript("Stop");

	if (started && newStart.valid())
	{
		sol::protected_function_result result = newStart();

		if (!result.valid())
		{
			sol::error error = result;
			std::cerr << "[Lua] " << error.what() << '\n';
			return false;
		}
	}

	environment = std::move(newEnvironment);
	startFunc = std::move(newStart);
	updateFunc = std::move(newUpdate);

	loaded = true;

	std::error_code ec;
	auto writeTime = std::filesystem::last_write_time(scriptPath, ec);

	if (!ec)
	{
		lastWriteTime = writeTime;
		hasWriteTime = true;
	}

	std::cout << "[Lua] Loaded: " << scriptPath << '\n';

	return true;
}

void LuaScript::OnStart()
{
	started = true;

	if (!loaded || !startFunc.valid()) return;

	sol::protected_function_result result = startFunc();

	if (!result.valid())
	{
		sol::error error = result;
		std::cerr << "[Lua] " << error.what() << '\n';
	}
}

void LuaScript::CheckHotReload()
{
#ifndef _RELEASE
	if (scriptPath.empty()) return;

	std::error_code ec;

	auto writeTime = std::filesystem::last_write_time(scriptPath, ec);

	if (ec) return;

	if (!hasWriteTime || writeTime != lastWriteTime)
	{
		lastWriteTime = writeTime;
		hasWriteTime = true;
		Reload();
	}
#endif
}

void LuaScript::OnUpdate()
{
	UpdateSensors();
	CheckHotReload();

	if (!loaded || !updateFunc.valid()) return;

	sol::protected_function_result result = updateFunc(Game::Time::deltaTime);

	if (!result.valid())
	{
		sol::error error = result;
		std::cerr << "[Lua] " << error.what() << '\n';
		CallScript("Stop");
		loaded = false;
	}
}

void LuaScript::CallScript(const char* name)
{
	if (!loaded) return;
	sol::object object = environment[name];
	if (object.get_type() != sol::type::function) return;
	sol::protected_function function = object.as<sol::protected_function>();
	sol::protected_function_result result = function();
	if (!result.valid())
	{
		sol::error error = result;
		std::cerr << "[Lua] " << error.what() << '\n';
	}
}
