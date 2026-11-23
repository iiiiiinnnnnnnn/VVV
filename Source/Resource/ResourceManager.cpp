#include <ProfileKit/ProfileKit.h>
#include "Core/Foundation/Logger.h"
#include "Resource/ResourceManager.h"
#include "Core/Foundation/BinaryFile.h"
#include "Rendering/Core/Graphics.h"
#include "Resource/ResourcePath.h"
#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
#include "Resource/CacheBuilder.h"
#endif

#include <Windows.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <format>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <sstream>
#include <system_error>
#include <unordered_set>

static constexpr const char* ResourceSection = "[resources]";

bool ResourceManager::PrepareGameResources()
{
	PROFILE_FUNCTION();
	if (resourcesPrepared) return true;
	errors.clear();
	assetPaths.clear();
	assetPathLookup.clear();
	models.clear();
	textures.clear();
	files.clear();
	preloadedFiles.clear();

	runtimeResourceRoot = GetRuntimeResourceRoot();
	cachedPathList = runtimeResourceRoot / "ResourceManifest.ini";

	if (!LoadCachedPathList())
		throw std::runtime_error(errors.empty()
			? "Resource manifest load failed."
			: errors.back());

	resourcesPrepared = PreloadConfiguredResources();
	return resourcesPrepared;
}

bool ResourceManager::RefreshResources(const std::filesystem::path& savedSource)
{
	PROFILE_FUNCTION();
#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
	try
	{
		const auto sourceRoot = FindSourceResourceRoot();
		if (sourceRoot.empty()) return true;
		std::unordered_map<std::string, std::string> previousUpdates;
		for (const auto& asset : assetPaths)
			previousUpdates[CacheSettings::Key(asset.path)] = asset.updated;
		if (runtimeResourceRoot.empty()) runtimeResourceRoot = GetRuntimeResourceRoot();
		cachedPathList = runtimeResourceRoot / "ResourceManifest.ini";
		const auto changed = sourceRoot == runtimeResourceRoot ? std::vector<std::string>{}
			: CacheBuilder::Build(sourceRoot, runtimeResourceRoot, false, savedSource);
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
		{ errors.push_back(exception.what()); Log(errors.back()); }
		return false;
	}
#else
	return true;
#endif
}

// 実行用パスを編集用の正本へ解決
std::filesystem::path ResourceManager::ResolveSourcePath(const std::filesystem::path& path)
{
	if (path.empty()) return path;
	const auto sourceRoot = FindSourceResourceRoot();
	const auto runtime = GetRuntimeResourceRoot();
	if (path.is_relative() && *path.begin() == "Resources")
		return (sourceRoot.empty() ? runtime : sourceRoot) / path.lexically_relative("Resources");
	if (sourceRoot.empty()) return path;
	const auto absolute = std::filesystem::weakly_canonical(std::filesystem::absolute(path));
	const auto relative = absolute.lexically_relative(runtime);
	if (!relative.empty() && !relative.is_absolute() && *relative.begin() != "..")
		return sourceRoot / relative;
	return path;
}

std::filesystem::path ResourceManager::GetRuntimeResourceRoot()
{
	std::wstring executable(32768, L'\0');
	const DWORD length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
	if (!length || length >= executable.size()) throw std::runtime_error("Cannot locate the executable.");
	executable.resize(length);
	return std::filesystem::weakly_canonical(std::filesystem::path(executable).parent_path() / "Resources");
}

std::filesystem::path ResourceManager::GetResourceRoot()
{
	const auto source = FindSourceResourceRoot();
	return source.empty() ? GetRuntimeResourceRoot() : source;
}

std::filesystem::path ResourceManager::GetResourceDirectory(ResourceType type)
{
	return GetResourceRoot() / GetPathSettings(type).directory;
}

std::string ResourceManager::PortableResourcePath(const std::filesystem::path& path)
{
	if (path.empty()) return {};
	const auto absolute = std::filesystem::weakly_canonical(std::filesystem::absolute(path));
	for (const auto& root : {GetResourceRoot(), GetRuntimeResourceRoot()})
	{
		const auto relative = absolute.lexically_relative(root);
		if (!relative.empty() && !relative.is_absolute() && *relative.begin() != "..")
			return ResourcePath::Text(std::filesystem::path("Resources") / relative);
	}
	return ResourcePath::Text(path.lexically_normal());
}

std::filesystem::path ResourceManager::ResolveEditPath(std::string_view value, ResourceType type) const
{
	if (value.empty()) return {};
	const auto portable = ResourcePath::Path(PortableResourcePath(ResourcePath::Path(value)));
	auto path = ResolveSourcePath(ExpandResourcePath(ResourcePath::Text(portable), type));
	std::error_code error;
	if (!std::filesystem::exists(path, error))
		path = ResolveSourcePath(ResourcePath::Path(ResolveResourcePath(ResourcePath::Text(portable), type)));
	return std::filesystem::weakly_canonical(std::filesystem::absolute(path));
}

std::filesystem::path ResourceManager::ResolveSavePath(const std::filesystem::path& path)
{
	if (path.empty()) throw std::invalid_argument("Resource save path is empty.");
	const auto root = GetResourceRoot();
	const auto portable = ResourcePath::Path(PortableResourcePath(path));
	if (portable.is_relative() && *portable.begin() == "Resources")
		return root / portable.lexically_relative("Resources");
	if (FindSourceResourceRoot().empty() && path.is_absolute()) return path.lexically_normal();
	if (path.is_relative() && path.has_parent_path() && *path.begin() != "..") return root / path;
	// プロジェクト外への保存指定も、リソースの種類に応じた正本へまとめる。
	for (size_t i = 0; i <= static_cast<size_t>(ResourceType::TerrainBrush); ++i)
	{
		const auto& settings = GetPathSettings(static_cast<ResourceType>(i));
		if (path.extension() == settings.extension) return root / settings.directory / path.filename();
	}
	if (CacheSettings::IsImage(ResourcePath::Text(path)))
		return root / GetPathSettings(ResourceType::Texture).directory / path.filename();
	return root / path.filename();
}

std::filesystem::path ResourceManager::SaveResourceFile(const std::filesystem::path& path,
	const std::function<void(const std::filesystem::path&)>& writer)
{
	const auto source = ResolveSavePath(path);
	std::filesystem::create_directories(source.parent_path());
	const auto staging = source.parent_path() / (source.stem().wstring() + L".resourcewriting" + source.extension().wstring());
	try
	{
		writer(staging);
		if (!std::filesystem::is_regular_file(staging)) throw std::runtime_error("Resource writer did not create a file.");
		if (!MoveFileExW(staging.c_str(), source.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
			throw std::runtime_error("Cannot replace project resource: " + ResourcePath::Text(source));
	}
	catch (...)
	{
		std::error_code ignored;
		std::filesystem::remove(staging, ignored);
		throw;
	}
	const auto relative = source.lexically_relative(GetResourceRoot());
	if (!relative.empty() && !relative.is_absolute() && *relative.begin() != "..")
	{
		const auto destination = GetRuntimeResourceRoot() / relative;
		if (source != destination)
		{
			const auto temporary = std::filesystem::path(destination.wstring() + L".resource.tmp");
			try
			{
				std::filesystem::create_directories(destination.parent_path());
				std::filesystem::copy_file(source, temporary, std::filesystem::copy_options::overwrite_existing);
				if (!MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
					throw std::runtime_error("Cannot replace runtime resource: " + ResourcePath::Text(destination));
			}
			catch (const std::exception& exception)
			{
				std::error_code ignored;
				std::filesystem::remove(temporary, ignored);
				throw std::runtime_error("Project resource saved, but runtime synchronization failed: " + std::string(exception.what()));
			}
		}
	}
	if (!RefreshResources(source)) throw std::runtime_error("Project resource saved, but runtime cache refresh failed.");
	return source;
}

std::filesystem::path ResourceManager::SaveResourceFile(std::string_view value, ResourceType type,
	const std::function<void(const std::filesystem::path&)>& writer)
{
	auto path = ResolveEditPath(value, type);
	if (!FindSourceResourceRoot().empty() && !PortableResourcePath(path).starts_with("Resources/"))
		path = GetResourceDirectory(type) / path.filename();
	return SaveResourceFile(path, writer);
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
		throw std::runtime_error(
			"Preload resource is not in Resources/ResourceManifest.ini: " + NormalizePath(path));

	bool loaded = true;
	if (value->type == AssetType::VMDLModel)
	{
		// モデルをキャッシュ
		loaded = GetModelResource(path) != nullptr;
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
	PROFILE_FUNCTION();
	bool success = true;
	for (const auto& asset : assetPaths)
	{
		const auto options = cacheSettings.Get(asset.path);
		if (!options.excluded && options.preload && !PreloadFile(asset.path)) success = false;
	}
	return success;
}

std::shared_ptr<const std::vector<uint8_t>> ResourceManager::LoadFile(const std::string& path)
{
	const auto resolved = ResolvePath(path);
	if (cacheSettings.Get(resolved).excluded) return nullptr;
	const auto key = CacheSettings::Key(resolved);
	if (const auto found = files.find(key); found != files.end()) return found->second;
	auto file = BinaryFile::Read(resolved);
	if (!file.IsValid()) throw std::runtime_error("Resource read failed: " + resolved);
	auto bytes = std::make_shared<std::vector<uint8_t>>(file.TakeData());
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
		{ errors.push_back("Generated cache not found: " + normalizedPath); Log(errors.back()); }
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

const ResourceManager::PathSettings& ResourceManager::GetPathSettings(ResourceType type)
{
	static constexpr std::array<PathSettings, 16> settings = {{
		{"Model", ".vmdl"},
		{"Effect", ".efkpkg"},
		{"Stage", ".vstg"},
		{"Sound", ".vsnd"},
		{"Animator", ".animator"},
		{"AI", ".lua"},
		{"Image", ".png"},
		{"Shader", ".cso"},
		{"Font", ".ttf"},
		{"Terrain/Decals", ".png"},
		{"Movie", ".vmovie"},
		{"Model", ".vmsh"},
		{"", ".physicslayers"},
		{"Input", ".vis"},
		{"Terrain/Layers", ".png"},
		{"Terrain/Brushes", ".png"}}};
	static_assert(settings.size() == static_cast<size_t>(ResourceType::TerrainBrush) + 1);
	const size_t index = static_cast<size_t>(type);
	if (index >= settings.size()) throw std::invalid_argument("Invalid resource type.");
	return settings[index];
}

std::filesystem::path ResourceManager::ExpandResourcePath(std::string_view value, ResourceType type)
{
	if (value.empty()) return {};
	const auto [directory, extension] = GetPathSettings(type);
	auto path = ResourcePath::Path(value).lexically_normal();
	if (!path.has_extension()) path += extension;
	if (path.is_absolute() || *path.begin() == "Resources") return path;
	const auto category = ResourcePath::Path(directory);
	const std::string first = ResourcePath::Text(*path.begin());
	const bool rooted = (!category.empty() && first == ResourcePath::Text(*category.begin())) ||
		(type == ResourceType::Texture && (first == "UI" || first == "Image" || first == "Texture" ||
			first == "Terrain" || first == "Sky" || first == "Model"));
	if (rooted) return std::filesystem::path("Resources") / path;
	if (first == ResourcePath::Text(category.filename()))
		return std::filesystem::path("Resources") / category.parent_path() / path;
	return std::filesystem::path("Resources") / category / path;
}

std::string ResourceManager::ResolveResourcePath(std::string_view value, ResourceType type) const
{
	if (value.empty()) return {};
	const auto [directory, extension] = GetPathSettings(type);

	const auto path = ExpandResourcePath(value, type);
	if (path.is_absolute()) return ResourcePath::Text(path);
	auto relative = ResourcePath::Path(value).lexically_normal();
	if (!relative.has_extension()) relative += extension;
	std::error_code error;
	const std::string expanded = ResourcePath::Text(path);
	const std::string resolved = ResolvePath(expanded);
	if (std::filesystem::exists(path, error)) return expanded;
	if (std::filesystem::exists(ResourcePath::Path(resolved), error)) return resolved;

	// Use the manifest for names in subdirectories, including textures outside Image.
	const std::string suffix = "/" + MakeLookupKey(ResourcePath::Text(relative));
	const std::string prefix = "Resources/" + std::string(directory) + "/";
	std::string match;
	for (const auto& asset : assetPaths)
	{
		std::string ext = ResourcePath::Text(ResourcePath::Path(asset.path).extension());
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
		if (type == ResourceType::Texture || type == ResourceType::DecalTex ||
			type == ResourceType::TerrainLayer || type == ResourceType::TerrainBrush)
		{
			if (type != ResourceType::Texture && !asset.path.starts_with(prefix)) continue;
			if (ext != ".png" && ext != ".dds" && ext != ".jpg" && ext != ".jpeg" &&
				ext != ".bmp" && ext != ".tga" && ext != ".hdr") continue;
		}
		else if (!asset.path.starts_with(prefix) || (ext != extension &&
			!(type == ResourceType::Font && ext == ".otf") &&
			!(type == ResourceType::Sound && ext == ".wav"))) continue;
		if (!MakeLookupKey(asset.path).ends_with(suffix)) continue;
		if (!match.empty() && match != asset.path)
			throw std::runtime_error("Ambiguous resource name: " + std::string(value) +
				" (" + match + " / " + asset.path + ")");
		match = asset.path;
	}
	return match.empty() ? expanded : match;
}

std::string ResourceManager::ShortResourcePath(std::string_view value, ResourceType type)
{
	const auto directory = GetPathSettings(type).directory;
	auto path = ResourcePath::Path(PortableResourcePath(ResourcePath::Path(value))).lexically_normal();
	const std::string text = ResourcePath::Text(path);
	const std::string prefix = std::string("Resources/") + directory + (*directory ? "/" : "");
	if (text.starts_with(prefix)) path = ResourcePath::Path(text.substr(prefix.size()));
	const auto extension = path.extension();
	if (extension == GetPathSettings(type).extension || extension == ".wav" || extension == ".vsnd" || extension == ".vmdl" ||
		extension == ".vstg" || extension == ".efkpkg" || extension == ".animator" ||
		extension == ".lua" || extension == ".png" || extension == ".dds" ||
		extension == ".ttf" || extension == ".otf" || extension == ".cso")
		path.replace_extension();
	return ResourcePath::Text(path);
}

std::shared_ptr<const VMDLModel> ResourceManager::GetModelResource(const std::string& key)
{
	PROFILE_FUNCTION();
	const std::string modelPath = ResolveResourcePath(PortableResourcePath(ResourcePath::Path(key)), ResourceType::Model);
	if (cacheSettings.Get(ResolvePath(modelPath)).excluded) return nullptr;
	const std::string lookupKey = MakeLookupKey(modelPath);
	const auto pathIt = assetPathLookup.find(lookupKey);
	if (pathIt == assetPathLookup.end() || assetPaths[pathIt->second].type != AssetType::VMDLModel)
		throw std::runtime_error(
			"VMDLModel is not in Resources/ResourceManifest.ini: " + NormalizePath(key));

	auto it = models.find(lookupKey);
	if (it == models.end())
	{
		const std::string& vmdlPath = assetPaths[pathIt->second].path;
		if (!std::filesystem::exists(vmdlPath))
			throw std::runtime_error("VMDLModel not found: " + vmdlPath);

		try
		{
			auto model = std::make_shared<VMDLModel>(vmdlPath.c_str());
			it = models.emplace(lookupKey, std::move(model)).first;
		}
		catch (const std::exception& exception)
		{
			throw std::runtime_error(
				"VMDLModel load failed: " + vmdlPath + " (" + exception.what() + ")");
		}
	}

	return it->second;
}

std::shared_ptr<VMDLModel> ResourceManager::LoadModel(const std::string& key, bool runtimeInstance)
{
	PROFILE_FUNCTION();
	const auto loadStarted = std::chrono::steady_clock::now();
	auto source = GetModelResource(key);
	if (!source) return {};
	auto result = runtimeInstance ? source->CloneRuntimeInstance(true) : source->Clone();
	const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - loadStarted).count();
	if (milliseconds >= 50)
	{
		const std::string message = "[SceneLoad] Model " + key + ": " +
			std::to_string(milliseconds) + " ms\n";
		Log(message.c_str());
	}
	return result;
}

std::span<const uint8_t> ResourceManager::LoadEmbeddedResource(int IDR_ID)
{
	const HMODULE module = GetModuleHandleW(nullptr);
	const HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(IDR_ID), MAKEINTRESOURCEW(10));
	if (!resource) throw std::runtime_error("Embedded resource not found: " + std::to_string(IDR_ID));
	const HGLOBAL loaded = LoadResource(module, resource);
	const auto* data = static_cast<const uint8_t*>(loaded ? LockResource(loaded) : nullptr);
	const DWORD size = SizeofResource(module, resource);
	if (!data || !size) throw std::runtime_error("Embedded resource could not be read: " + std::to_string(IDR_ID));
	return {data, size};
}

std::shared_ptr<Texture> ResourceManager::LoadTexture(const std::string& key, ResourceType type)
{
	PROFILE_FUNCTION();
	if (key.empty()) return {};
	const std::string resolvedPath = ResolvePath(ResolveResourcePath(key, type));
	if (cacheSettings.Get(resolvedPath).excluded) return nullptr;
	const std::string lookupKey = CacheSettings::Key(resolvedPath);
	auto it = textures.find(lookupKey);
	if (it == textures.end())
	{
		auto texture = std::make_shared<Texture>(Game::Graphics::Instance().GetDevice(), resolvedPath.c_str());
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
		{ errors.push_back("Resources/ResourceManifest.ini was not found. Release builds never create caches."); Log(errors.back()); }
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
			{ errors.push_back("Resources/ResourceManifest.ini contains an invalid line: " + line); Log(errors.back()); }
			continue;
		}

		const std::string name = line.substr(0, separator);
		const std::string value = line.substr(separator + 1);
		if (name == "updated")
		{
			if (!lastAsset)
			{
				{ errors.push_back("Resources/ResourceManifest.ini contains updated before a resource."); Log(errors.back()); }
				continue;
			}
			lastAsset->updated = value;
			continue;
		}

		if (name == "preload")
		{
			if (!lastAsset || (value != "0" && value != "1"))
			{
				{ errors.push_back("ResourceManifest.ini contains an invalid preload entry."); Log(errors.back()); }
				continue;
			}
			cacheSettings.Set(lastAsset->path, {false, value == "1"});
			continue;
		}

		AssetType type;
		if (!ParseTypeName(name, type))
		{
			{ errors.push_back("Resources/ResourceManifest.ini contains an unknown type: " + name); Log(errors.back()); }
			continue;
		}
		if (AddAssetPath(type, value)) lastAsset = &assetPaths.back();
	}

	if (!inResourceSection && assetPaths.empty())
		{ errors.push_back("Resources/ResourceManifest.ini has no [resources] section."); Log(errors.back()); }
	return errors.empty();
}

// 動的生成したリソースの登録時も先読み設定を維持して保存
bool ResourceManager::SaveCachedPathList()
{
	std::ofstream file(cachedPathList, std::ios::trunc);
	if (!file)
	{
		{ errors.push_back("Resources/ResourceManifest.ini could not be written."); Log(errors.back()); }
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
	{ errors.push_back("Resources/ResourceManifest.ini write failed."); Log(errors.back()); }
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
			{ errors.push_back("Duplicate resource name: " + lookupKey); Log(errors.back()); }
			return false;
		}
		assetPathLookup[lookupKey] = assetPaths.size();
	}

	assetPaths.push_back(std::move(asset));
	return true;
}


std::filesystem::path ResourceManager::FindSourceResourceRoot()
{
	for (const auto& start : {GetRuntimeResourceRoot().parent_path(), std::filesystem::current_path()})
	{
		for (auto directory = start; !directory.empty(); directory = directory.parent_path())
		{
			std::error_code error;
			for (std::filesystem::directory_iterator file(directory, error), end;
				file != end && !error; file.increment(error))
			{
				if (file->is_regular_file(error) && file->path().extension() == ".sln")
					return std::filesystem::weakly_canonical(directory / "Resources");
			}
			if (directory == directory.root_path()) break;
		}
	}
	return {};
}

std::string ResourceManager::NormalizePath(const std::string& path)
{
	return std::filesystem::path(path).lexically_normal().generic_string();
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
