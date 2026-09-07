// ResourceManager.h
#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Resource/VMDLModel.h"
#include "Resource/Texture.h"
#include "Resource/CacheSettings.h"

class ResourceManager
{
public:
	enum class AssetType
	{
		File,
		VMDLModel,
		MipmapTexture,
	};

	struct AssetPath
	{
		AssetType type = AssetType::File;
		std::string path;
		std::string updated;
	};

private:
	ResourceManager() = default;
	~ResourceManager() = default;

public:
	static ResourceManager& Instance()
	{
		static ResourceManager instance;
		return instance;
	}

	static std::filesystem::path FindSourceResourceRoot();
	static std::filesystem::path ResolveSourcePath(const std::filesystem::path& path);

	bool PrepareGameResources();
	bool RefreshResources(const std::filesystem::path& savedSource = {});
	bool AreGameResourcesPrepared() const { return resourcesPrepared; }
	// 一度だけ読込
	bool PreloadFile(const std::string& path);
    bool PreloadConfiguredResources();
    std::shared_ptr<const std::vector<uint8_t>> LoadFile(const std::string& path);
	void RegisterGeneratedCache(const std::string& path);
	std::string ResolvePath(const std::string& path) const;
	const std::vector<std::string>& GetErrors() const { return errors; }
	const std::vector<AssetPath>& GetAssetPaths() const { return assetPaths; }

	std::shared_ptr<VMDLModel> LoadModel(const std::string& key);
	std::shared_ptr<Texture> LoadTexture(const std::string& key);

private:
	bool LoadCachedPathList();
	bool SaveCachedPathList();
	bool AddAssetPath(AssetType type, const std::filesystem::path& path, const std::string& updated = {});
	void ReportError(const std::string& message);

	static std::string NormalizePath(const std::string& path);
	static std::string MakeLookupKey(const std::string& path);
	static std::string GetLastWriteTimeText(const std::filesystem::path& path);
	static const char* ToTypeName(AssetType type);
	static bool ParseTypeName(const std::string& name, AssetType& type);

	std::filesystem::path runtimeResourceRoot;
	std::filesystem::path cachedPathList;
	std::vector<AssetPath> assetPaths;
	std::unordered_map<std::string, size_t> assetPathLookup;
	std::unordered_map<std::string, std::shared_ptr<VMDLModel>> models;
	std::unordered_map<std::string, std::shared_ptr<Texture>> textures;
	// 読込済み一覧
	std::unordered_set<std::string> preloadedFiles;
	std::vector<std::string> errors;
	bool resourcesPrepared = false;
    CacheSettings cacheSettings;
    std::unordered_map<std::string, std::shared_ptr<const std::vector<uint8_t>>> files;
};
