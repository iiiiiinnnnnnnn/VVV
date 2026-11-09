#include <ProfileKit/ProfileKit.h>
#include "Core/Foundation/Logger.h"
#include "Gameplay/AI/LuaManager.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <system_error>
#include <utility>

#include "Resource/ResourceManager.h"
#include "Resource/ResourcePath.h"

LuaManager& LuaManager::Instance()
{
	static LuaManager instance;
	return instance;
}

std::string LuaManager::ResolveScriptPath(const std::string& path)
{
	auto resolved = ResourcePath::Path(path);
	if (!resolved.is_absolute())
		resolved = ResourceManager::ResolveSourcePath(ResourcePath::Path(
			ResourceManager::Instance().ResolveResourcePath(path, ResourceManager::ResourceType::AI)));
	std::string key = ResourcePath::Text(std::filesystem::absolute(resolved).lexically_normal());
	for (char& character : key)
		if (character >= 'A' && character <= 'Z') character += 'a' - 'A';
	return key;
}

std::shared_ptr<const LuaManager::ScriptSource> LuaManager::GetScript(const std::string& path)
{
	if (path.empty()) return {};
	const std::lock_guard lock(mutex);
	// LuaScriptは正規化済みのパスを保持するため、更新時はファイルへアクセスしない。
	if (const auto found = scripts.find(path); found != scripts.end()) return found->second.source;
	const std::string key = ResolveScriptPath(path);
	auto [found, inserted] = scripts.try_emplace(key);
	if (inserted)
	{
		std::error_code error;
		const auto writeTime = std::filesystem::last_write_time(ResourcePath::Path(key), error);
		if (!error) ReadScript(key, found->second, writeTime);
		else Log(std::string("[Lua] Script was not found: ") + key);
	}
	return found->second.source;
}

std::shared_ptr<const LuaManager::ScriptSource> LuaManager::Reload(const std::string& path)
{
	if (path.empty()) return {};
	const std::lock_guard lock(mutex);
	const std::string key = ResolveScriptPath(path);
	auto& script = scripts[key];
	std::error_code error;
	const auto writeTime = std::filesystem::last_write_time(ResourcePath::Path(key), error);
	if (error || !ReadScript(key, script, writeTime)) return {};
	return script.source;
}

bool LuaManager::ReadScript(const std::string& path, CachedScript& script,
	std::filesystem::file_time_type writeTime)
{
	PROFILE_FUNCTION();
	// 読み込み失敗時も同じ更新日時への再試行を繰り返さない。
	script.writeTime = writeTime;
	script.hasWriteTime = true;
	std::ifstream input(ResourcePath::Path(path), std::ios::binary);
	if (!input)
	{
		Log(std::string("[Lua] Failed to read: ") + path);
		return false;
	}
	std::string code((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
	if (input.bad()) return false;
	if (code.starts_with("\xEF\xBB\xBF")) code.erase(0, 3);
	// load_fileと同様にshebangを無視し、エラーの行番号は保つ。
	if (code.starts_with("#")) code.erase(0, code.find('\n'));
	auto source = std::make_shared<ScriptSource>();
	source->path = path;
	source->code = std::move(code);
	script.source = std::move(source);
	return true;
}

void LuaManager::Update()
{
	PROFILE_FUNCTION();
#ifndef _RELEASE
	const auto now = std::chrono::steady_clock::now();
	const std::lock_guard lock(mutex);
	if (now < nextCheck) return;
	nextCheck = now + std::chrono::milliseconds(500);
	for (auto& [path, script] : scripts)
	{
		std::error_code error;
		const auto writeTime = std::filesystem::last_write_time(ResourcePath::Path(path), error);
		if (error) continue;
		if (!script.hasWriteTime || writeTime != script.writeTime) ReadScript(path, script, writeTime);
	}
#endif
}
