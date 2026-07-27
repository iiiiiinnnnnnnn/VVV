// ResourceManager.cpp

#include "Resource/ResourceManager.h"

#include <algorithm>
#include <chrono>
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

std::filesystem::path ToDataPath(const std::filesystem::path& relativePath)
{
	return std::filesystem::path("Data") / relativePath;
}
}

ResourceManager& ResourceManager::Instance()
{
	static ResourceManager instance;
	return instance;
}

bool ResourceManager::PrepareGameResources()
{
	if (resourcesPrepared) return true;
	errors.clear();
	assetPaths.clear();
	assetPathLookup.clear();
	models.clear();
	textures.clear();
	mipmapTextures.clear();

	runtimeDataRoot = std::filesystem::current_path() / "Data";
	cachedPathList = runtimeDataRoot / "cached.ini";

	sourceDataRoot = FindSourceDataRoot();
	if (!sourceDataRoot.empty())
	{
		if (!BuildCaches()) return false;
		if (!SaveCachedPathList()) return false;
	}
	else if (!LoadCachedPathList()) return false;

	resourcesPrepared = PreloadCachedResources();
	return resourcesPrepared;
}

bool ResourceManager::ReloadGameResources()
{
	resourcesPrepared = false;
	return PrepareGameResources();
}

void ResourceManager::RegisterGeneratedCache(const std::string& path)
{
	const std::string normalizedPath = NormalizePath(path);
	if (!std::filesystem::exists(normalizedPath))
	{
		ReportError("Generated cache not found: " + normalizedPath);
		return;
	}

	for (const AssetPath& asset : assetPaths)
	{
		if (asset.path == normalizedPath) return;
	}
	if (AddAssetPath(AssetType::File, normalizedPath, GetLastWriteTimeText(normalizedPath))) SaveCachedPathList();
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
	const std::string lookupKey = MakeLookupKey(key);
	const auto pathIt = assetPathLookup.find(lookupKey);
	if (pathIt == assetPathLookup.end() || assetPaths[pathIt->second].type != AssetType::VMDLModel)
	{
		ReportError("VMDLModel is not in Data/cached.ini: " + NormalizePath(key));
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
	const std::string lookupKey = MakeLookupKey(key);
	const std::string resolvedPath = ResolvePath(key);
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

bool ResourceManager::BuildCaches()
{
	if (!std::filesystem::is_directory(sourceDataRoot))
	{
		ReportError("Source Data directory not found: " + sourceDataRoot.generic_string());
		return false;
	}

	std::error_code error;
	std::filesystem::create_directories(runtimeDataRoot, error);
	if (error)
	{
		ReportError("Runtime Data directory could not be created: " + runtimeDataRoot.generic_string());
		return false;
	}

	std::vector<std::filesystem::path> sourceFiles;
	for (std::filesystem::recursive_directory_iterator it(sourceDataRoot, error), end;
		it != end && !error;
		it.increment(error))
	{
		if (it->is_regular_file()) sourceFiles.push_back(it->path());
	}
	if (error)
	{
		ReportError("Source Data scan failed: " + error.message());
		return false;
	}

	std::sort(sourceFiles.begin(), sourceFiles.end());
	std::unordered_set<std::string> outputPaths;
	for (const std::filesystem::path& sourcePath : sourceFiles)
	{
		const std::filesystem::path relativePath = sourcePath.lexically_relative(sourceDataRoot);
		if (IsDevelopmentOnly(relativePath)) continue;

		std::filesystem::path runtimePath = runtimeDataRoot / relativePath;
		AssetType type = AssetType::File;
		if (IsModelSource(sourcePath))
		{
			type = AssetType::VMDLModel;
		}
		else if (IsTerrainLayerSource(relativePath))
		{
			type = AssetType::MipmapTexture;
			runtimePath.replace_extension(".dds");
		}

		const std::filesystem::path runtimeRelativePath = runtimePath.lexically_relative(runtimeDataRoot);
		const std::filesystem::path cachedPath = ToDataPath(runtimeRelativePath);
		const std::string outputKey = MakeLookupKey(cachedPath.generic_string());
		if ((type == AssetType::VMDLModel || type == AssetType::MipmapTexture) && !outputPaths.insert(outputKey).second)
		{
			ReportError("Duplicate resource name: " + outputKey);
			continue;
		}

		std::filesystem::create_directories(runtimePath.parent_path(), error);
		if (error)
		{
			ReportError("Cache directory could not be created: " + runtimePath.parent_path().generic_string());
			error.clear();
			continue;
		}

		if (type == AssetType::MipmapTexture)
		{
			const bool upToDate = std::filesystem::exists(runtimePath) &&
				std::filesystem::last_write_time(runtimePath) >= std::filesystem::last_write_time(sourcePath);
			if (!upToDate && FAILED(MipmapTexture::CreateDDSCache(sourcePath, runtimePath)))
			{
				ReportError("DDS cache creation failed: " + cachedPath.generic_string());
				continue;
			}
		}
		else
		{
			std::filesystem::copy_file(
				sourcePath,
				runtimePath,
				std::filesystem::copy_options::update_existing,
				error);
			if (error)
			{
				ReportError("Resource copy failed: " + cachedPath.generic_string() + " (" + error.message() + ")");
				error.clear();
				continue;
			}
		}

		if (!std::filesystem::exists(runtimePath))
		{
			ReportError("Cached resource was not created: " + cachedPath.generic_string());
			continue;
		}
		AddAssetPath(type, cachedPath, GetLastWriteTimeText(sourcePath));
	}

	std::vector<std::filesystem::path> staleCaches;
	for (std::filesystem::recursive_directory_iterator it(runtimeDataRoot, error), end;
		it != end && !error;
		it.increment(error))
	{
		if (!it->is_regular_file()) continue;
		const std::filesystem::path relativePath = it->path().lexically_relative(runtimeDataRoot);
		const bool isModelCache = it->path().extension() == ".vmdl";
		const bool isLayerCache = it->path().extension() == ".dds" &&
			relativePath.parent_path().generic_string() == "Terrain/Layers";
		if (!isModelCache && !isLayerCache) continue;

		const std::string outputKey = MakeLookupKey(ToDataPath(relativePath).generic_string());
		if (!outputPaths.contains(outputKey)) staleCaches.push_back(it->path());
	}
	if (error)
	{
		ReportError("Stale cache scan failed: " + error.message());
		error.clear();
	}
	for (const std::filesystem::path& staleCache : staleCaches)
	{
		std::filesystem::remove(staleCache, error);
		if (!error) continue;
		ReportError("Stale cache could not be removed: " + staleCache.generic_string() + " (" + error.message() + ")");
		error.clear();
	}

	std::vector<std::filesystem::path> generatedCaches;
	for (std::filesystem::recursive_directory_iterator it(runtimeDataRoot, error), end;
		it != end && !error;
		it.increment(error))
	{
		if (it->is_regular_file() && it->path().extension() == ".vx") generatedCaches.push_back(it->path());
	}
	std::sort(generatedCaches.begin(), generatedCaches.end());
	for (const std::filesystem::path& generatedCache : generatedCaches)
	{
		AddAssetPath(
			AssetType::File,
			ToDataPath(generatedCache.lexically_relative(runtimeDataRoot)),
			GetLastWriteTimeText(generatedCache));
	}

	return errors.empty();
}

bool ResourceManager::LoadCachedPathList()
{
	std::ifstream file(cachedPathList);
	if (!file)
	{
		ReportError("Data/cached.ini was not found. Release builds never create caches.");
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
			ReportError("Data/cached.ini contains an invalid line: " + line);
			continue;
		}

		const std::string name = line.substr(0, separator);
		const std::string value = line.substr(separator + 1);
		if (name == "updated")
		{
			if (!lastAsset)
			{
				ReportError("Data/cached.ini contains updated before a resource.");
				continue;
			}
			lastAsset->updated = value;
			continue;
		}

		AssetType type;
		if (!ParseTypeName(name, type))
		{
			ReportError("Data/cached.ini contains an unknown type: " + name);
			continue;
		}
		if (AddAssetPath(type, value)) lastAsset = &assetPaths.back();
	}

	if (!inResourceSection && assetPaths.empty()) ReportError("Data/cached.ini has no [resources] section.");
	return errors.empty();
}

bool ResourceManager::SaveCachedPathList()
{
	std::ofstream file(cachedPathList, std::ios::trunc);
	if (!file)
	{
		ReportError("Data/cached.ini could not be written.");
		return false;
	}

	file << ResourceSection << '\n';
	for (const AssetPath& asset : assetPaths)
	{
		file << ToTypeName(asset.type) << '=' << asset.path << '\n';
		if (!asset.updated.empty()) file << "updated=" << asset.updated << '\n';
	}
	if (file.good()) return true;
	ReportError("Data/cached.ini write failed.");
	return false;
}

bool ResourceManager::PreloadCachedResources()
{
	for (const AssetPath& asset : assetPaths)
	{
		if (!std::filesystem::exists(asset.path))
		{
			ReportError("Resource listed in Data/cached.ini was not found: " + asset.path);
			continue;
		}

		if (asset.type == AssetType::VMDLModel)
		{
			LoadModel(asset.path);
		}
		else if (asset.type == AssetType::MipmapTexture)
		{
			auto texture = std::make_shared<MipmapTexture>();
			if (!texture->Load(asset.path.c_str()))
			{
				ReportError("Mipmap texture cache load failed: " + asset.path);
				continue;
			}
			mipmapTextures.push_back(std::move(texture));
		}
	}
	return errors.empty();
}

bool ResourceManager::AddAssetPath(
	AssetType type,
	const std::filesystem::path& path,
	const std::string& updated)
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

std::filesystem::path ResourceManager::FindSourceDataRoot()
{
	for (std::filesystem::path directory = std::filesystem::current_path();
		!directory.empty();
		directory = directory.parent_path())
	{
		if (std::filesystem::exists(directory / "Game.sln") &&
			std::filesystem::is_directory(directory / "Data"))
		{
			return std::filesystem::weakly_canonical(directory / "Data");
		}
		if (directory == directory.root_path()) break;
	}
	return {};
}

std::string ResourceManager::NormalizePath(const std::string& path)
{
	return std::filesystem::path(path).lexically_normal().generic_string();
}

std::string ResourceManager::MakeLookupKey(const std::string& path)
{
	std::filesystem::path lookupPath = std::filesystem::path(path).lexically_normal();
	lookupPath.replace_extension();
	std::string key = lookupPath.generic_string();
	std::transform(key.begin(), key.end(), key.begin(), ::tolower);
	return key;
}

std::string ResourceManager::GetLastWriteTimeText(const std::filesystem::path& path)
{
	std::error_code error;
	const auto fileTime = std::filesystem::last_write_time(path, error);
	if (error) return {};

	const auto systemTime = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
		fileTime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
	const std::time_t time = std::chrono::system_clock::to_time_t(systemTime);
	std::tm utcTime{};
	if (gmtime_s(&utcTime, &time)) return {};

	std::ostringstream text;
	text << std::put_time(&utcTime, "%Y-%m-%dT%H:%M:%SZ");
	return text.str();
}

bool ResourceManager::IsModelSource(const std::filesystem::path& path)
{
	std::string extension = path.extension().string();
	std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
	return extension == ".vmdl";
}

bool ResourceManager::IsTerrainLayerSource(const std::filesystem::path& relativePath)
{
	if (relativePath.parent_path().generic_string() != "Terrain/Layers") return false;
	std::string extension = relativePath.extension().string();
	std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
	return extension == ".png" || extension == ".jpg" || extension == ".jpeg" ||
		extension == ".tga" || extension == ".bmp" || extension == ".hdr";
}

bool ResourceManager::IsDevelopmentOnly(const std::filesystem::path& relativePath)
{
	const std::string filename = relativePath.filename().string();
	if (filename == "cached" || filename == "cached.ini" || filename == "DDSAssistant.bat" ||
		filename == "texconv.exe" || filename == "FBX2glTF-windows-x64.exe" || filename == "FBX2glTF.bat")
	{
		return true;
	}

	std::string extension = relativePath.extension().string();
	std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
	if (extension == ".glb" || extension == ".gltf" || extension == ".vx" ||
		extension == ".physicslayers") return true;
	if (extension == ".dds" && relativePath.parent_path().generic_string() == "Terrain/Layers") return true;

	const std::string generic = relativePath.generic_string();
	if (generic.starts_with("VMDLModel/") && !IsModelSource(relativePath)) return true;
	for (const auto& part : relativePath)
	{
		if (part.string().starts_with("ninclude_")) return true;
	}
	return false;
}

const char* ResourceManager::ToTypeName(AssetType type)
{
	switch (type)
	{
	case AssetType::VMDLModel: return "model";
	case AssetType::MipmapTexture: return "mipmap";
	default: return "file";
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

