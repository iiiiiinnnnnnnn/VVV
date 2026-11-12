#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Resource/VMDLModel.h"
#include "Resource/Texture.h"
#include "Resource/CacheSettings.h"

class ResourceManager
{
public:
	enum class ResourceType
	{
		Model, Effect, Stage, Sound, Animator, AI, Texture, Shader, Font,
	};

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

#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
	void DrawGUI();
	void ReloadResourceList();
#endif

	bool PrepareGameResources();
	bool RefreshResources(const std::filesystem::path& savedSource = {});
	bool AreGameResourcesPrepared() const { return resourcesPrepared; }

	// 一度だけ読込
	bool PreloadFile(const std::string& path);
    bool PreloadConfiguredResources();
    std::shared_ptr<const std::vector<uint8_t>> LoadFile(const std::string& path);
	void RegisterGeneratedCache(const std::string& path);
	std::string ResolvePath(const std::string& path) const;
	std::string ResolveResourcePath(std::string_view value, ResourceType type) const;
	std::string ResolveShaderPath(std::string_view value) const
	{
		return ResolvePath(ResolveResourcePath(value, ResourceType::Shader));
	}
	static std::string ShortResourcePath(std::string_view value, const char* directory);
	const std::vector<std::string>& GetErrors() const { return errors; }
	const std::vector<AssetPath>& GetAssetPaths() const { return assetPaths; }

	std::shared_ptr<const VMDLModel> GetModelResource(const std::string& key);
	std::shared_ptr<VMDLModel> LoadModel(const std::string& key, bool runtimeInstance = false);
	std::shared_ptr<Texture> LoadTexture(const std::string& key, bool required = true);

private:
#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
	struct ResourceTreeNode
	{
		std::string name;
		std::string path;
		bool file = false;
		std::map<std::string, ResourceTreeNode> children;
	};

	void DrawTreeLabel(const ResourceTreeNode& node, int depth,
		const std::vector<bool>& guides, bool last);
	void AddResourceTreePath(ResourceTreeNode& root, const std::string& path);
	void DrawCacheTreeNode(ResourceTreeNode& node, int depth,
		const std::vector<bool>& guides, bool last);

	CacheSettings resourceGUISettings;
	std::vector<std::string> resourceGUIPaths;
	std::string resourceGUIMessage;
	bool resourceGUIInitialized = false;
#endif

	bool LoadCachedPathList();
	bool SaveCachedPathList();
	bool AddAssetPath(AssetType type, const std::filesystem::path& path, const std::string& updated = {});

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
