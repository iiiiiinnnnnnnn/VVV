// ResourceManager.cpp
#include "Resource/ResourceManager.h"
#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
#include "Resource/CacheBuilder.h"
#endif

#include <algorithm>
#include <chrono>
#include <format>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <unordered_set>
#include <windows.h>

namespace
{
constexpr const char* ResourceSection = "[resources]";

// フォルダー改名前の履歴を、移行先が存在する場合だけ新しいパスへ解決
std::filesystem::path ResolveRenamedResourcePath(const std::filesystem::path& path)
{
	if (path.empty() || std::filesystem::exists(path)) return path;
	std::filesystem::path migrated;
	bool afterResources = false;
	bool changed = false;
	for (const auto& part : path)
	{
		const auto name = CacheSettings::Key(part.generic_string());
		if (afterResources && (name == "vmdl" || name == "vstg"))
		{
			migrated /= name == "vmdl" ? "Model" : "Stage";
			changed = true;
		}
		else migrated /= part;
		afterResources = name == "resources";
	}
	return changed && std::filesystem::exists(migrated) ? migrated : path;
}

} // namespace

bool ResourceManager::PrepareGameResources()
{
	if (resourcesPrepared) return true;
	errors.clear();
	assetPaths.clear();
	assetPathLookup.clear();
	models.clear();
	textures.clear();
	files.clear();
	preloadedFiles.clear();

	runtimeResourceRoot = std::filesystem::current_path() / "Resources";
	cachedPathList = runtimeResourceRoot / "ResourceManifest.ini";

	if (!LoadCachedPathList()) return false;

	resourcesPrepared = PreloadConfiguredResources();
	return resourcesPrepared;
}

bool ResourceManager::RefreshResources(const std::filesystem::path& savedSource)
{
#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
	try
	{
		const auto sourceRoot = FindSourceResourceRoot();
		if (sourceRoot.empty()) return true;
		std::unordered_map<std::string, std::string> previousUpdates;
		for (const auto& asset : assetPaths)
			previousUpdates[CacheSettings::Key(asset.path)] = asset.updated;
		if (runtimeResourceRoot.empty()) runtimeResourceRoot = std::filesystem::current_path() / "Resources";
		cachedPathList = runtimeResourceRoot / "ResourceManifest.ini";
		const auto changed = CacheBuilder::Build(sourceRoot, runtimeResourceRoot, false, savedSource);
		for (const auto& path : changed)
		{
			const auto key = MakeLookupKey(path);
			models.erase(key);
			textures.erase(CacheSettings::Key(path));
			files.erase(CacheSettings::Key(path));
		}
		preloadedFiles.clear();
		assetPaths.clear();
		assetPathLookup.clear();
		errors.clear();
		resourcesPrepared = LoadCachedPathList();
		// ビルド側ですでに更新されたリソースも破棄
		for (const auto& asset : assetPaths)
		{
			const auto key = CacheSettings::Key(asset.path);
			const auto old = previousUpdates.find(key);
			if (old == previousUpdates.end()) continue;
			if (old->second == asset.updated) previousUpdates.erase(old);
		}
		for (const auto& [key, updated] : previousUpdates)
		{
			models.erase(MakeLookupKey(key));
			textures.erase(key);
			files.erase(key);
		}
		return resourcesPrepared && PreloadConfiguredResources();
	}
	catch (const std::exception& exception)
	{
		models.clear();
		textures.clear();
		files.clear();
		preloadedFiles.clear();
		ReportError(exception.what());
		return false;
	}
#else
	return true;
#endif
}

// 実行用パスを編集用の正本へ解決し、旧フォルダー名の履歴も移行
std::filesystem::path ResourceManager::ResolveSourcePath(const std::filesystem::path& requestedPath)
{
	const auto path = ResolveRenamedResourcePath(requestedPath);
#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
	const auto sourceRoot = FindSourceResourceRoot();
	if (sourceRoot.empty() || path.empty()) return path;
	const auto runtime = std::filesystem::weakly_canonical(std::filesystem::current_path() / "Resources");
	const auto absolute = std::filesystem::weakly_canonical(std::filesystem::absolute(path));
	const auto relative = absolute.lexically_relative(runtime);
	if (!relative.empty() && !relative.is_absolute() && *relative.begin() != "..")
		return ResolveRenamedResourcePath(sourceRoot / relative);
#endif
	return path;
}
bool ResourceManager::PreloadFile(const std::string& path)
{
	if (cacheSettings.Get(path).excluded) return false;
	// 拡張子込みで判定
	std::string preloadKey = NormalizePath(path);
	std::transform(preloadKey.begin(), preloadKey.end(), preloadKey.begin(), ::tolower);
	if (preloadedFiles.contains(preloadKey)) return true;

	const auto indexedAsset = assetPathLookup.find(MakeLookupKey(path));
	const AssetPath* value =
		indexedAsset == assetPathLookup.end() ? nullptr : &assetPaths[indexedAsset->second];
	if (!value)
	{
		// 一般ファイルを検索
		const std::string normalizedPath = NormalizePath(path);
		const auto found = std::find_if(assetPaths.begin(), assetPaths.end(),
			[&](const AssetPath& asset) { return NormalizePath(asset.path) == normalizedPath; });
		if (found != assetPaths.end()) value = &*found;
	}
	if (!value)
	{
		ReportError("Preload resource is not in Resources/ResourceManifest.ini: " + NormalizePath(path));
		return false;
	}

	bool loaded = true;
	if (value->type == AssetType::VMDLModel)
	{
		// モデルをキャッシュ
		loaded = LoadModel(path) != nullptr;
	}
	else
	{
		if (CacheSettings::IsImage(value->path))
		{
			// テクスチャをキャッシュ
			loaded = LoadTexture(path) != nullptr;
		}
		else
		{
			// その他はファイル本体を保持
			loaded = LoadFile(value->path) != nullptr;
		}
	}

	if (loaded) preloadedFiles.insert(preloadKey);
	return loaded;
}

// Manifestから読み込んだ先読み設定に従ってリソースを準備
bool ResourceManager::PreloadConfiguredResources()
{
	try
	{
		bool success = true;
		for (const auto& asset : assetPaths)
		{
			const auto options = cacheSettings.Get(asset.path);
			if (!options.excluded && options.preload && !PreloadFile(asset.path)) success = false;
		}
		return success;
	}
	catch (const std::exception& error)
	{
		ReportError(error.what());
		return false;
	}
}

std::shared_ptr<const std::vector<uint8_t>> ResourceManager::LoadFile(const std::string& path)
{
	const auto resolved = ResolvePath(path);
	if (cacheSettings.Get(resolved).excluded) return nullptr;
	const auto key = CacheSettings::Key(resolved);
	if (const auto found = files.find(key); found != files.end()) return found->second;
	std::ifstream input(resolved, std::ios::binary | std::ios::ate);
	if (!input) { ReportError("Resource not found: " + resolved); return nullptr; }
	const auto size = input.tellg();
	if (size < 0) { ReportError("Resource size failed: " + resolved); return nullptr; }
	auto bytes = std::make_shared<std::vector<uint8_t>>(static_cast<size_t>(size));
	input.seekg(0);
	if (!input.read(reinterpret_cast<char*>(bytes->data()), static_cast<std::streamsize>(bytes->size())))
	{
		ReportError("Resource read failed: " + resolved);
		return nullptr;
	}
	if (cacheSettings.Get(resolved).preload) files.emplace(key, bytes);
	return bytes;
}

void ResourceManager::RegisterGeneratedCache(const std::string& path)
{
#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
	const auto relative = std::filesystem::weakly_canonical(std::filesystem::absolute(path))
		.lexically_relative(std::filesystem::weakly_canonical(runtimeResourceRoot));
	if (relative.empty() || relative.is_absolute() || *relative.begin() == "..")
	{
		RefreshResources(path);
		return;
	}
	const auto normalizedPath = (std::filesystem::path("Resources") / relative).generic_string();
	if (!std::filesystem::exists(normalizedPath))
	{
		ReportError("Generated cache not found: " + normalizedPath);
		return;
	}
	for (auto& asset : assetPaths)
	{
		if (asset.path != normalizedPath) continue;
		asset.updated = GetLastWriteTimeText(normalizedPath);
		SaveCachedPathList();
		return;
	}
	if (AddAssetPath(AssetType::File, normalizedPath, GetLastWriteTimeText(normalizedPath)))
		SaveCachedPathList();
#endif
}
std::string ResourceManager::ResolvePath(const std::string& path) const
{
	const std::string lookupKey = MakeLookupKey(path);
	const auto it = assetPathLookup.find(lookupKey);
	if (it == assetPathLookup.end()) return NormalizePath(path);
	return assetPaths[it->second].path;
}

std::shared_ptr<VMDLModel> ResourceManager::LoadModel(const std::string& key)
{
	if (cacheSettings.Get(ResolvePath(key)).excluded) return nullptr;
	const std::string lookupKey = MakeLookupKey(key);
	const auto pathIt = assetPathLookup.find(lookupKey);
	if (pathIt == assetPathLookup.end() || assetPaths[pathIt->second].type != AssetType::VMDLModel)
	{
		ReportError("VMDLModel is not in Resources/ResourceManifest.ini: " + NormalizePath(key));
		return nullptr;
	}

	auto it = models.find(lookupKey);
	if (it == models.end())
	{
		const std::string& vmdlPath = assetPaths[pathIt->second].path;
		if (!std::filesystem::exists(vmdlPath))
		{
			ReportError("VMDLModel not found: " + vmdlPath);
			return nullptr;
		}

		try
		{
			auto model = std::make_shared<VMDLModel>(vmdlPath.c_str());
			it = models.emplace(lookupKey, std::move(model)).first;
		}
		catch (const std::exception& exception)
		{
			ReportError("VMDLModel load failed: " + vmdlPath + " (" + exception.what() + ")");
			return nullptr;
		}
	}

	return it->second->Clone();
}

std::shared_ptr<Texture> ResourceManager::LoadTexture(const std::string& key)
{
	const std::string resolvedPath = ResolvePath(key);
	if (cacheSettings.Get(resolvedPath).excluded) return nullptr;
	const std::string lookupKey = CacheSettings::Key(resolvedPath);
	auto it = textures.find(lookupKey);
	if (it == textures.end())
	{
		if (!std::filesystem::exists(resolvedPath))
		{
			ReportError("Texture not found: " + resolvedPath);
			return nullptr;
		}

		auto texture = std::make_shared<Texture>(resolvedPath.c_str());
		it = textures.emplace(lookupKey, std::move(texture)).first;
	}

	return it->second->Clone();
}

// 実行用一覧と先読み設定をManifestから読み込む
bool ResourceManager::LoadCachedPathList()
{
	cacheSettings = CacheSettings{};
	std::ifstream file(cachedPathList);
	if (!file)
	{
		ReportError("Resources/ResourceManifest.ini was not found. Release builds never create caches.");
		return false;
	}

	bool inResourceSection = false;
	AssetPath* lastAsset = nullptr;
	std::string line;
	while (std::getline(file, line))
	{
		if (!line.empty() && line.back() == '\r') line.pop_back();
		if (line.empty() || line[0] == ';' || line[0] == '#') continue;
		if (line.front() == '[')
		{
			inResourceSection = line == ResourceSection;
			continue;
		}
		if (!inResourceSection) continue;

		const size_t separator = line.find('=');
		if (separator == std::string::npos)
		{
			ReportError("Resources/ResourceManifest.ini contains an invalid line: " + line);
			continue;
		}

		const std::string name = line.substr(0, separator);
		const std::string value = line.substr(separator + 1);
		if (name == "updated")
		{
			if (!lastAsset)
			{
				ReportError("Resources/ResourceManifest.ini contains updated before a resource.");
				continue;
			}
			lastAsset->updated = value;
			continue;
		}

		if (name == "preload")
		{
			if (!lastAsset || (value != "0" && value != "1"))
			{
				ReportError("ResourceManifest.ini contains an invalid preload entry.");
				continue;
			}
			cacheSettings.Set(lastAsset->path, {false, value == "1"});
			continue;
		}

		AssetType type;
		if (!ParseTypeName(name, type))
		{
			ReportError("Resources/ResourceManifest.ini contains an unknown type: " + name);
			continue;
		}
		if (AddAssetPath(type, value)) lastAsset = &assetPaths.back();
	}

	if (!inResourceSection && assetPaths.empty())
		ReportError("Resources/ResourceManifest.ini has no [resources] section.");
	return errors.empty();
}

// 動的生成したリソースの登録時も先読み設定を維持して保存
bool ResourceManager::SaveCachedPathList()
{
	std::ofstream file(cachedPathList, std::ios::trunc);
	if (!file)
	{
		ReportError("Resources/ResourceManifest.ini could not be written.");
		return false;
	}

	file << ResourceSection << '\n';
	for (const AssetPath& asset : assetPaths)
	{
		file << ToTypeName(asset.type) << '=' << asset.path << '\n';
		if (!asset.updated.empty()) file << "updated=" << asset.updated << '\n';
		file << "preload=" << cacheSettings.Get(asset.path).preload << '\n';
	}
	if (file.good()) return true;
	ReportError("Resources/ResourceManifest.ini write failed.");
	return false;
}

bool ResourceManager::AddAssetPath(
	AssetType type, const std::filesystem::path& path, const std::string& updated)
{
	AssetPath asset;
	asset.type = type;
	asset.path = NormalizePath(path.generic_string());
	asset.updated = updated;

	if (type == AssetType::VMDLModel || type == AssetType::MipmapTexture)
	{
		const std::string lookupKey = MakeLookupKey(asset.path);
		const auto existing = assetPathLookup.find(lookupKey);
		if (existing != assetPathLookup.end())
		{
			if (assetPaths[existing->second].path == asset.path) return true;
			ReportError("Duplicate resource name: " + lookupKey);
			return false;
		}
		assetPathLookup[lookupKey] = assetPaths.size();
	}

	assetPaths.push_back(std::move(asset));
	return true;
}

void ResourceManager::ReportError(const std::string& message)
{
	errors.push_back(message);
	const std::string output = "[ResourceManager] " + message + "\n";
	OutputDebugStringA(output.c_str());
}

std::filesystem::path ResourceManager::FindSourceResourceRoot()
{
	for (std::filesystem::path directory = std::filesystem::current_path(); !directory.empty();
		directory = directory.parent_path())
	{
		if (std::filesystem::exists(directory / "Game.sln") &&
			std::filesystem::is_directory(directory / "Resources"))
		{
			return std::filesystem::weakly_canonical(directory / "Resources");
		}
		if (directory == directory.root_path()) break;
	}
	return {};
}

std::string ResourceManager::NormalizePath(const std::string& path)
{
	std::string normalized = std::filesystem::path(path).lexically_normal().generic_string();
	std::string lower = normalized;
	std::transform(lower.begin(), lower.end(), lower.begin(),
		[](unsigned char value) { return static_cast<char>(std::tolower(value)); });
	if (lower == "data") return "Resources";
	if (lower.starts_with("data/")) normalized.replace(0, 4, "Resources");
	return normalized;
}

std::string ResourceManager::MakeLookupKey(const std::string& path)
{
	std::filesystem::path lookupPath = NormalizePath(path);
	lookupPath.replace_extension();
	std::string key = lookupPath.generic_string();
	std::transform(key.begin(), key.end(), key.begin(), ::tolower);
	return key;
}

std::string ResourceManager::GetLastWriteTimeText(const std::filesystem::path& path)
{
	std::error_code error;
	const auto time = std::filesystem::last_write_time(path, error);
	if (error) return {};
	return std::format("{:%FT%TZ}", std::chrono::clock_cast<std::chrono::system_clock>(time));
}
const char* ResourceManager::ToTypeName(AssetType type)
{
	switch (type)
	{
	case AssetType::VMDLModel:
		return "model";
	case AssetType::MipmapTexture:
		return "mipmap";
	default:
		return "file";
	}
}

bool ResourceManager::ParseTypeName(const std::string& name, AssetType& type)
{
	if (name == "model") type = AssetType::VMDLModel;
	else if (name == "mipmap") type = AssetType::MipmapTexture;
	else if (name == "file") type = AssetType::File;
	else return false;
	return true;
}
