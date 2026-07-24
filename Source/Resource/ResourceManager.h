// ResourceManager.h

#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Resource/VMDLModel.h"
#include "Resource/Texture.h"

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
	static ResourceManager& Instance();

	static std::filesystem::path FindSourceDataRoot();

	bool PrepareGameResources();
	void RegisterGeneratedCache(const std::string& path);
	std::string ResolvePath(const std::string& path) const;
	const std::vector<std::string>& GetErrors() const { return errors; }
	const std::vector<AssetPath>& GetAssetPaths() const { return assetPaths; }

	std::shared_ptr<VMDLModel> LoadModel(const std::string& key);
	std::shared_ptr<Texture> LoadTexture(const std::string& key);

private:
	bool BuildCaches();
	bool LoadCachedPathList();
	bool SaveCachedPathList();
	bool PreloadCachedResources();
	bool AddAssetPath(AssetType type, const std::filesystem::path& path, const std::string& updated = {});
	void ReportError(const std::string& message);

	static std::string NormalizePath(const std::string& path);
	static std::string MakeLookupKey(const std::string& path);
	static std::string GetLastWriteTimeText(const std::filesystem::path& path);
	static bool IsModelSource(const std::filesystem::path& path);
	static bool IsTerrainLayerSource(const std::filesystem::path& relativePath);
	static bool IsDevelopmentOnly(const std::filesystem::path& relativePath);
	static const char* ToTypeName(AssetType type);
	static bool ParseTypeName(const std::string& name, AssetType& type);

	std::filesystem::path sourceDataRoot;
	std::filesystem::path runtimeDataRoot;
	std::filesystem::path cachedPathList;
	std::vector<AssetPath> assetPaths;
	std::unordered_map<std::string, size_t> assetPathLookup;
	std::unordered_map<std::string, std::shared_ptr<VMDLModel>> models;
	std::unordered_map<std::string, std::shared_ptr<Texture>> textures;
	std::vector<std::shared_ptr<MipmapTexture>> mipmapTextures;
	std::vector<std::string> errors;
};
