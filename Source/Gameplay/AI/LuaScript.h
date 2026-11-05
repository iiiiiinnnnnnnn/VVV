
#pragma once

#include <sol/sol.hpp>

#include <memory>
#include <string>

#include "Core/Object/Component.h"
#include "Gameplay/AI/LuaManager.h"

class LuaScript : public Component
{
  public:
	void OnAwake() override;
	explicit LuaScript(Object* owner);
	~LuaScript() override;

	// Resources/AI...
	bool Load(const std::string& path);
	bool Reload();

	void OnStart() override;
	void OnUpdate() override;

	const char* GetDebugName() const override { return "LuaScript"; }

  protected:
	bool HasScriptPath() const { return !scriptPath.empty(); }
	virtual void BindEnvironment(sol::environment&) {}
	virtual void UpdateSensors() {}
	void CallScript(const char* name);

  private:
	bool ApplyScript(std::shared_ptr<const LuaManager::ScriptSource> source);
	bool CreateEnvironment(const LuaManager::ScriptSource& source,
		sol::environment& environment, sol::protected_function& start,
		sol::protected_function& update);

	void CheckHotReload();

  private:
	std::string scriptPath;

	std::unique_ptr<sol::state> lua;

	sol::environment environment;
	sol::protected_function startFunc;
	sol::protected_function updateFunc;

	std::shared_ptr<const LuaManager::ScriptSource> scriptSource;
	bool started = false;
	bool loaded = false;
};
