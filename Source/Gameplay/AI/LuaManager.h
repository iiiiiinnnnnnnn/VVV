#pragma once

#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

class LuaManager
{
public:
	struct ScriptSource
	{
		std::string path;
		std::string code;
	};

	static LuaManager& Instance();
	std::shared_ptr<const ScriptSource> GetScript(const std::string& path);
	std::shared_ptr<const ScriptSource> Reload(const std::string& path);
	void Update();

private:
	struct CachedScript
	{
		std::shared_ptr<const ScriptSource> source;
		std::filesystem::file_time_type writeTime{};
		bool hasWriteTime = false;
	};

	LuaManager() = default;
	static std::string ResolveScriptPath(const std::string& path);
	bool ReadScript(const std::string& path, CachedScript& script,
		std::filesystem::file_time_type writeTime);

	std::mutex mutex;
	std::unordered_map<std::string, CachedScript> scripts;
	std::chrono::steady_clock::time_point nextCheck{};
};
