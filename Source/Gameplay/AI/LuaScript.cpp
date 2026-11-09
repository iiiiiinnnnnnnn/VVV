#include <ProfileKit/ProfileKit.h>
#include "Core/Foundation/Logger.h"

#include "LuaScript.h"

#include <iostream>
#include <utility>

#include "Application/Time/GameTime.h"

LuaScript::LuaScript(Object* owner) : Component(owner)
{
}

void LuaScript::OnAwake()
{
	if (lua) return;
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
	PROFILE_FUNCTION();
	scriptPath = path;
	return ApplyScript(LuaManager::Instance().GetScript(path));
}

bool LuaScript::CreateEnvironment(const LuaManager::ScriptSource& source,
	sol::environment& newEnvironment,
	sol::protected_function& newStart, sol::protected_function& newUpdate)
{
	sol::load_result chunk = lua->load(source.code, "@" + source.path);

	if (!chunk.valid())
	{
		sol::error error = chunk;
		Log(std::string("[Lua] ") + error.what());
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
		Log(std::string("[Lua] ") + error.what());
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
	return ApplyScript(LuaManager::Instance().Reload(scriptPath));
}

bool LuaScript::ApplyScript(std::shared_ptr<const LuaManager::ScriptSource> source)
{
	PROFILE_FUNCTION();
	if (!source) return false;
	LuaScript::OnAwake();
	// 構文エラーも同じ内容を毎フレーム再コンパイルせず、次の変更を待つ。
	scriptPath = source->path;
	scriptSource = source;

	sol::environment newEnvironment;
	sol::protected_function newStart;
	sol::protected_function newUpdate;

	if (!CreateEnvironment(*source, newEnvironment, newStart, newUpdate))
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
			Log(std::string("[Lua] ") + error.what());
			return false;
		}
	}

	environment = std::move(newEnvironment);
	startFunc = std::move(newStart);
	updateFunc = std::move(newUpdate);

	loaded = true;

	Log(std::string("[Lua] Loaded: ") + scriptPath);

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
		Log(std::string("[Lua] ") + error.what());
	}
}

void LuaScript::CheckHotReload()
{
	PROFILE_FUNCTION();
#ifndef _RELEASE
	if (scriptPath.empty()) return;
	auto source = LuaManager::Instance().GetScript(scriptPath);
	if (source && source != scriptSource) ApplyScript(std::move(source));
#endif
}

void LuaScript::OnUpdate()
{
	PROFILE_FUNCTION();
	UpdateSensors();
	CheckHotReload();

	if (!loaded || !updateFunc.valid()) return;

	sol::protected_function_result result = updateFunc(Game::Time::deltaTime);

	if (!result.valid())
	{
		sol::error error = result;
		Log(std::string("[Lua] ") + error.what());
		CallScript("Stop");
		loaded = false;
	}
}

void LuaScript::CallScript(const char* name)
{
	PROFILE_FUNCTION();
	if (!loaded) return;
	sol::object object = environment[name];
	if (object.get_type() != sol::type::function) return;
	sol::protected_function function = object.as<sol::protected_function>();
	sol::protected_function_result result = function();
	if (!result.valid())
	{
		sol::error error = result;
		Log(std::string("[Lua] ") + error.what());
	}
}
