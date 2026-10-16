
#pragma once

#include <sol/sol.hpp>

#include <filesystem>
#include <memory>
#include <string>

#include "Core/Object/Component.h"

class LuaScript : public Component
{
  public:
	explicit LuaScript(Object* owner);
	~LuaScript() override;

	bool Load(const std::string& path);
	bool Reload();

	void OnStart() override;
	void OnUpdate() override;

	const char* GetDebugName() const override { return "LuaScript"; }

  protected:
	virtual void BindEnvironment(sol::environment&) {}
	virtual void UpdateSensors() {}
	void CallScript(const char* name);

  private:
	bool CreateEnvironment(sol::environment& environment, sol::protected_function& start,
		sol::protected_function& update);

	void CheckHotReload();

  private:
	std::string scriptPath;

	std::unique_ptr<sol::state> lua;

	sol::environment environment;
	sol::protected_function startFunc;
	sol::protected_function updateFunc;

	std::filesystem::file_time_type lastWriteTime{};

	bool hasWriteTime = false;
	bool started = false;
	bool loaded = false;
};
