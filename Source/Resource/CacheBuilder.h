// CacheBuilder.h
#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <windows.h>

// ゲームと開発ツールで共有するキャッシュ生成
class CacheBuilder
{
public:
	static std::vector<std::string> ListResources(const std::filesystem::path& sourceRoot);

	// 更新分を生成し、変更した実行用パスを返す
	static std::vector<std::string> Build(const std::filesystem::path& sourceRoot,
		const std::filesystem::path& outputRoot, bool force = false,
		const std::filesystem::path& savedSource = {});

	// ミップマップ付きDDSを生成
	static HRESULT CreateDDSCache(const std::filesystem::path& source, const std::filesystem::path& destination);
};
