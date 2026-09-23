// CacheBuilder.cpp
#include "Resource/CacheBuilder.h"
#include "Resource/CacheSettings.h"
#include "Audio/SoundTrackRegistry.h"
#include "Audio/VSoundFormat.h"

#include <combaseapi.h>
#include <compressapi.h>
#include <DirectXTex.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <format>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace
{
namespace fs = std::filesystem;

struct Entry
{
	std::string type, updated;
	fs::path relative;
	bool preload = false;
};

struct Asset
{
	fs::path source;
	std::vector<std::pair<int, fs::path>> soundVariants;
	Entry entry;
	bool rebuild = false;
	bool sound = false;
};

using Manifest = std::map<std::string, Entry>;

struct ComScope
{
	HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	~ComScope() { if (SUCCEEDED(result)) CoUninitialize(); }
};
// パスをUTF-8に変換
std::string Text(const fs::path& path)
{
	const auto value = path.generic_u8string();
	return {value.begin(), value.end()};
}

fs::path Path(const std::string& text)
{
	return fs::path(std::u8string(text.begin(), text.end()));
}

std::string Lower(std::string value)
{
	std::transform(value.begin(), value.end(), value.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return value;
}

std::string Trim(const std::string& value)
{
	const auto first = value.find_first_not_of(" \t\r\n");
	if (first == std::string::npos) return {};
	return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}

// 出力用の相対パスを検証
fs::path RelativePath(const std::string& value)
{
	const fs::path path = Path(value);
	if (path.empty() || path.is_absolute() || path.has_root_name() ||
		value.find_first_of(":*?\r\n\t") != std::string::npos)
		throw std::runtime_error("Invalid relative path: " + value);
	for (const auto& part : path)
		if (part == "..") throw std::runtime_error("Parent paths are not allowed: " + value);
	return path.lexically_normal();
}

fs::path Destination(const fs::path& root, const fs::path& relative)
{
	return root / RelativePath(Text(relative));
}

// 生成済みvxの形式とサイズを検証
void ValidateVx(const fs::path& path)
{
	std::ifstream input(path, std::ios::binary);
	std::array<uint32_t, 4> header{};
	input.read(reinterpret_cast<char*>(header.data()), sizeof(header));
	if (!input || header[0] != 0x58565656 || header[1] != 3 || !header[2] || !header[3] || header[3] % 3)
		throw std::runtime_error("Invalid terrain vx header: " + Text(path));
	// 現行v3は72バイトのヘッダー、頂点float3、インデックスuint32
	const uint64_t expected = 72ull + uint64_t(header[2]) * 12 + uint64_t(header[3]) * 4;
	if (fs::file_size(path) != expected) throw std::runtime_error("Invalid terrain vx size: " + Text(path));
}

// 素材の更新時刻を端数まで記録
std::string Updated(const fs::path& path)
{
	return std::format("{:%FT%TZ}", std::chrono::clock_cast<std::chrono::system_clock>(fs::last_write_time(path)));
}

bool IsExcluded(const fs::path& relative)
{
	for (const auto& part : relative)
		if (Lower(Text(part)).starts_with("ninclude_")) return true;
	const auto name = Lower(Text(relative.filename()));
	const auto extension = Lower(Text(relative.extension()));
	if (name == "resourcemanifest.ini" || name == "resourcesettings.ini" || name == "cached" ||
		name == "nothing" || name == "editor.ini" ||
		Lower(Text(relative)) == "sound/tracks.ini") return true;
	if (extension == ".glb" || extension == ".gltf" || extension == ".bat" ||
		extension == ".exe" || extension == ".pdb" || extension == ".tmp" ||
		extension == ".wav") return true;
	return extension == ".dds" && Lower(Text(relative)).starts_with("terrain/layers/");
}

int IndexedWaveVariant(const fs::path& path, const std::string& baseStem)
{
	const std::string stem = Text(path.stem());
	if (Lower(stem) == Lower(baseStem)) return 0;
	if (stem.size() <= baseStem.size() + 2 ||
		Lower(stem.substr(0, baseStem.size())) != Lower(baseStem) ||
		stem[baseStem.size()] != '[' || stem.back() != ']') return -1;
	const std::string number = stem.substr(baseStem.size() + 1, stem.size() - baseStem.size() - 2);
	if (number.empty() || !std::all_of(number.begin(), number.end(),
		[](unsigned char c) { return std::isdigit(c) != 0; })) return -1;
	try
	{
		const unsigned long value = std::stoul(number);
		return value <= static_cast<unsigned long>(std::numeric_limits<int32_t>::max())
			? static_cast<int>(value) : -1;
	}
	catch (...) { return -1; }
}

std::vector<std::pair<int, fs::path>> FindSoundVariants(
	const fs::path& sourceRoot, const std::string& registeredPath)
{
	fs::path relative = Path(registeredPath);
	if (relative.is_absolute())
		relative = fs::weakly_canonical(relative).lexically_relative(sourceRoot);
	else
	{
		auto iterator = relative.begin();
		if (iterator != relative.end() && Lower(Text(*iterator)) == "resources")
		{
			fs::path withoutRoot;
			for (++iterator; iterator != relative.end(); ++iterator) withoutRoot /= *iterator;
			relative = withoutRoot;
		}
	}
	if (Lower(Text(relative.extension())) != ".wav")
		throw std::runtime_error("Sound track must reference WAV: " + registeredPath);
	const fs::path selected = sourceRoot / relative;
	std::string baseStem = Text(selected.stem());
	const size_t bracket = baseStem.rfind('[');
	if (bracket != std::string::npos && baseStem.back() == ']')
	{
		const std::string number = baseStem.substr(bracket + 1, baseStem.size() - bracket - 2);
		if (!number.empty() && std::all_of(number.begin(), number.end(),
			[](unsigned char c) { return std::isdigit(c) != 0; })) baseStem.resize(bracket);
	}

	std::map<int, fs::path> variants;
	const fs::path directory = selected.parent_path();
	if (!fs::is_directory(directory))
		throw std::runtime_error("Sound track directory is missing: " + Text(directory));
	for (const auto& file : fs::directory_iterator(directory))
	{
		if (!file.is_regular_file() || Lower(Text(file.path().extension())) != ".wav") continue;
		const int variant = IndexedWaveVariant(file.path(), baseStem);
		if (variant < 0) continue;
		if (!variants.emplace(variant, file.path()).second)
			throw std::runtime_error("Duplicate sound variant " + std::to_string(variant) + ": " + registeredPath);
	}
	if (variants.empty()) throw std::runtime_error("Sound track WAV was not found: " + registeredPath);
	return {variants.begin(), variants.end()};
}

std::string SoundUpdated(const Asset& asset)
{
	std::ostringstream value;
	for (const auto& [variant, path] : asset.soundVariants)
		value << variant << ':' << Text(path) << ':' << Updated(path) << ':' << fs::file_size(path) << ';';
	return value.str();
}

std::vector<uint8_t> ReadBytes(const fs::path& path)
{
	std::ifstream input(path, std::ios::binary | std::ios::ate);
	if (!input) throw std::runtime_error("Cannot read sound: " + Text(path));
	const auto size = input.tellg();
	if (size <= 0 || static_cast<uint64_t>(size) > std::numeric_limits<uint32_t>::max())
		throw std::runtime_error("Invalid sound size: " + Text(path));
	std::vector<uint8_t> bytes(static_cast<size_t>(size));
	input.seekg(0);
	if (!input.read(reinterpret_cast<char*>(bytes.data()), size))
		throw std::runtime_error("Sound read failed: " + Text(path));
	if (bytes.size() < 12 || std::memcmp(bytes.data(), "RIFF", 4) != 0 ||
		std::memcmp(bytes.data() + 8, "WAVE", 4) != 0)
		throw std::runtime_error("Invalid WAV: " + Text(path));
	return bytes;
}

void WriteVSound(const Asset& asset, const fs::path& destination)
{
	COMPRESSOR_HANDLE compressor = nullptr;
	if (!CreateCompressor(COMPRESS_ALGORITHM_XPRESS_HUFF, nullptr, &compressor))
		throw std::runtime_error("Sound compressor initialization failed");
	struct CompressorScope
	{
		COMPRESSOR_HANDLE value;
		~CompressorScope() { if (value) CloseCompressor(value); }
	} scope{compressor};

	std::vector<VSound::Entry> entries;
	std::vector<std::vector<uint8_t>> compressedBlocks;
	entries.reserve(asset.soundVariants.size());
	compressedBlocks.reserve(asset.soundVariants.size());
	uint64_t offset = sizeof(uint32_t) * 3 + asset.soundVariants.size() *
		(sizeof(int32_t) + sizeof(uint32_t) * 2 + sizeof(uint64_t));
	for (const auto& [variant, path] : asset.soundVariants)
	{
		const auto source = ReadBytes(path);
		SIZE_T required = 0;
		Compress(compressor, source.data(), source.size(), nullptr, 0, &required);
		if (required == 0) throw std::runtime_error("Sound compression sizing failed: " + Text(path));
		std::vector<uint8_t> compressed(required);
		SIZE_T written = 0;
		if (!Compress(compressor, source.data(), source.size(), compressed.data(), compressed.size(), &written))
			throw std::runtime_error("Sound compression failed: " + Text(path));
		compressed.resize(written);
		entries.push_back({variant, static_cast<uint32_t>(source.size()),
			static_cast<uint32_t>(compressed.size()), offset});
		offset += compressed.size();
		compressedBlocks.push_back(std::move(compressed));
	}

	std::ofstream output(destination, std::ios::binary | std::ios::trunc);
	if (!output) throw std::runtime_error("Cannot write VSND: " + Text(destination));
	const uint32_t magic = VSound::Magic;
	const uint32_t version = VSound::Version;
	const uint32_t count = static_cast<uint32_t>(entries.size());
	output.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
	output.write(reinterpret_cast<const char*>(&version), sizeof(version));
	output.write(reinterpret_cast<const char*>(&count), sizeof(count));
	for (const auto& entry : entries)
	{
		output.write(reinterpret_cast<const char*>(&entry.variant), sizeof(entry.variant));
		output.write(reinterpret_cast<const char*>(&entry.uncompressedSize), sizeof(entry.uncompressedSize));
		output.write(reinterpret_cast<const char*>(&entry.compressedSize), sizeof(entry.compressedSize));
		output.write(reinterpret_cast<const char*>(&entry.offset), sizeof(entry.offset));
	}
	for (const auto& block : compressedBlocks)
		output.write(reinterpret_cast<const char*>(block.data()), static_cast<std::streamsize>(block.size()));
	if (!output) throw std::runtime_error("VSND write failed: " + Text(destination));
}

std::vector<Asset> Collect(const fs::path& sourceRoot)
{
	std::map<std::string, Asset> assets;
	std::set<std::string> typedKeys;
	auto add = [&](const fs::path& root, const fs::path& path, bool prebuilt) {
		const auto relative = path.lexically_relative(root);
		if (!prebuilt && IsExcluded(relative)) return;
		const auto extension = Lower(Text(relative.extension()));
		const bool mipmap = !prebuilt && Lower(Text(relative)).starts_with("terrain/layers/");
		if (mipmap && extension != ".png" && extension != ".jpg" && extension != ".jpeg" &&
			extension != ".tga" && extension != ".bmp" && extension != ".hdr")
			throw std::runtime_error("Unsupported terrain image: " + Text(path));
		if (prebuilt) ValidateVx(path);
		Entry entry{mipmap ? "mipmap" : extension == ".vmdl" ? "model" : "file", {}, relative};
		if (mipmap) entry.relative.replace_extension(".dds");
		const auto key = Lower(Text(entry.relative));
		if (assets.contains(key)) throw std::runtime_error("Duplicate output: " + key);
		if (entry.type != "file")
		{
			auto typedPath = entry.relative;
			typedPath.replace_extension();
			if (!typedKeys.insert(Lower(Text(typedPath))).second)
				throw std::runtime_error("Duplicate resource lookup key: " + Text(typedPath));
		}
		assets.emplace(key, Asset{path, {}, entry});
	};
	for (const auto& file : fs::recursive_directory_iterator(sourceRoot))
		if (file.is_regular_file())
			add(sourceRoot, file.path(), Lower(Text(file.path().extension())) == ".vx");

	SoundTrackRegistry registry;
	registry.Load(sourceRoot / "Sound" / "tracks.ini");
	for (const auto& [track, trackEntry] : registry.GetEntries())
	{
		Asset asset;
		asset.sound = true;
		asset.soundVariants = FindSoundVariants(sourceRoot, trackEntry.path);
		asset.source = asset.soundVariants.front().second;
		asset.entry.type = "file";
		asset.entry.relative = fs::path("Sound") / "Tracks" /
			(std::to_string(track) + VSound::Extension);
		const auto key = Lower(Text(asset.entry.relative));
		if (assets.contains(key)) throw std::runtime_error("Duplicate output: " + key);
		assets.emplace(key, std::move(asset));
	}
	std::vector<Asset> result;
	for (auto& [key, asset] : assets) result.push_back(std::move(asset));
	return result;
}

Manifest ReadManifest(const fs::path& output)
{
	Manifest entries;
	const auto path = Destination(output, "ResourceManifest.ini");
	if (!fs::exists(path)) return entries;
	std::ifstream input(path);
	if (!input) throw std::runtime_error("Cannot read ResourceManifest.ini");
	Entry* current = nullptr;
	bool inResources = false;
	std::string line;
	while (std::getline(input, line))
	{
		if (line.starts_with("\xef\xbb\xbf")) line.erase(0, 3);
		line = Trim(line);
		if (line.empty() || line[0] == ';' || line[0] == '#') continue;
		if (line[0] == '[') { inResources = line == "[resources]"; current = nullptr; continue; }
		if (!inResources) continue;
		const auto separator = line.find('=');
		if (separator == std::string::npos) throw std::runtime_error("Invalid ResourceManifest.ini entry");
		const auto type = line.substr(0, separator);
		const auto value = line.substr(separator + 1);
		if (type == "updated")
		{
			if (current) current->updated = value;
			continue;
		}
		if (type == "preload")
		{
			if (!current || (value != "0" && value != "1"))
				throw std::runtime_error("Invalid ResourceManifest.ini preload entry");
			current->preload = value == "1";
			continue;
		}
		if ((type != "model" && type != "mipmap" && type != "file") || !value.starts_with("Resources/"))
			throw std::runtime_error("Invalid ResourceManifest.ini resource: " + value);
		const auto relative = RelativePath(value.substr(10));
		if (Lower(Text(relative)) == "resourcemanifest.ini") throw std::runtime_error("ResourceManifest.ini cannot list itself");
		Destination(output, relative);
		auto [found, inserted] = entries.emplace(Lower(Text(relative)), Entry{type, {}, relative});
		if (!inserted) throw std::runtime_error("Duplicate ResourceManifest.ini resource: " + value);
		current = &found->second;
	}
	if (!input.eof()) throw std::runtime_error("ResourceManifest.ini read failed");
	return entries;
}

std::string ManifestText(const Manifest& entries)
{
	std::ostringstream text;
	text << "[resources]\n";
	for (const auto& [key, entry] : entries)
		text << entry.type << "=Resources/" << Text(entry.relative) << "\nupdated=" << entry.updated
			<< "\npreload=" << entry.preload << '\n';
	return text.str();
}
// 一時ファイルから置換、失敗時は旧ファイルを維持
void ReplaceFile(const fs::path& temporary, const fs::path& destination)
{
	if (!MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
		throw std::runtime_error("Cannot replace: " + Text(destination) + " (" + std::to_string(GetLastError()) + ")");
}

void WriteText(const fs::path& path, const std::string& text)
{
	if (fs::exists(path))
	{
		std::ifstream current(path, std::ios::binary);
		const std::string old((std::istreambuf_iterator<char>(current)), {});
		if (old == text) return;
	}
	fs::create_directories(path.parent_path());
	const fs::path temporary = path.wstring() + L".cachebuilder.tmp";
	std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
	output.write(text.data(), static_cast<std::streamsize>(text.size()));
	output.close();
	if (!output) throw std::runtime_error("Write failed: " + Text(temporary));
	ReplaceFile(temporary, path);
}

}

HRESULT CacheBuilder::CreateDDSCache(const fs::path& source, const fs::path& destination)
{
	// WICが保持するCOMオブジェクトをスレッド終了まで有効にする
	thread_local const ComScope com;
	if (FAILED(com.result) && com.result != RPC_E_CHANGED_MODE) return com.result;
	DirectX::TexMetadata metadata{};
	DirectX::ScratchImage image, mipmaps;
	const auto extension = Lower(Text(source.extension()));
	HRESULT result;
	if (extension == ".dds") result = DirectX::LoadFromDDSFile(source.c_str(), DirectX::DDS_FLAGS_NONE, &metadata, image);
	else if (extension == ".tga") result = DirectX::LoadFromTGAFile(source.c_str(), &metadata, image);
	else if (extension == ".hdr") result = DirectX::LoadFromHDRFile(source.c_str(), &metadata, image);
	else result = DirectX::LoadFromWICFile(source.c_str(), DirectX::WIC_FLAGS_NONE, &metadata, image);
	if (FAILED(result)) return result;
	result = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), metadata,
		DirectX::TEX_FILTER_DEFAULT, 0, mipmaps);
	if (FAILED(result)) return result;
	std::error_code error;
	if (!destination.parent_path().empty()) fs::create_directories(destination.parent_path(), error);
	if (error) return E_FAIL;
	return DirectX::SaveToDDSFile(mipmaps.GetImages(), mipmaps.GetImageCount(), mipmaps.GetMetadata(),
		DirectX::DDS_FLAGS_NONE, destination.c_str());
}

std::vector<std::string> CacheBuilder::Build(const fs::path& sourceRoot, const fs::path& outputRoot,
	bool force, const fs::path& savedSource)
{
	const auto source = fs::weakly_canonical(fs::absolute(sourceRoot));
	const auto output = fs::weakly_canonical(fs::absolute(outputRoot));
	if (!fs::is_directory(source)) throw std::runtime_error("Source Resources is missing");
	if (source == output) throw std::runtime_error("Source and output directories must be different");
	const auto saved = savedSource.empty() ? fs::path{} : fs::weakly_canonical(fs::absolute(savedSource));
	auto assets = Collect(source);
	CacheSettings settings;
	settings.Load(source / "ResourceSettings.ini");
	std::erase_if(assets, [&](const Asset& asset) {
		return settings.Get("Resources/" + Text(asset.entry.relative)).excluded;
	});
	auto previous = ReadManifest(output);
	Manifest next;
	std::vector<std::string> changed;
	bool invalidated = false;
	for (auto& asset : assets)
	{
		auto& entry = asset.entry;
		entry.preload = settings.Get("Resources/" + Text(entry.relative)).preload;
		const auto key = Lower(Text(entry.relative));
		const auto destination = Destination(output, entry.relative);
		entry.updated = asset.sound ? SoundUpdated(asset) : Updated(asset.source);
		const auto old = previous.find(key);
		asset.rebuild = force || (!saved.empty() && fs::equivalent(asset.source, saved)) ||
			old == previous.end() || old->second.type != entry.type || old->second.updated != entry.updated ||
			!fs::is_regular_file(destination) || (!asset.sound && Updated(destination) != entry.updated) ||
			(entry.type != "mipmap" && fs::file_size(asset.source) != fs::file_size(destination));
		if (asset.sound)
		{
			asset.rebuild = force || old == previous.end() || old->second.type != entry.type ||
				old->second.updated != entry.updated || !fs::is_regular_file(destination);
			if (!saved.empty())
				for (const auto& [variant, variantSource] : asset.soundVariants)
					if (fs::equivalent(variantSource, saved)) asset.rebuild = true;
		}
		if (!asset.rebuild) continue;
		previous[key] = entry;
		previous[key].updated.clear();
		invalidated = true;
	}
	// 失敗しても次回再生成できるよう先に時刻を消す
	const auto manifestPath = Destination(output, "ResourceManifest.ini");
	if (invalidated) WriteText(manifestPath, ManifestText(previous));
	size_t converted = 0, copied = 0, skipped = 0, removed = 0;
	for (const auto& asset : assets)
	{
		const auto& entry = asset.entry;
		const auto destination = Destination(output, entry.relative);
		if (asset.rebuild)
		{
			fs::create_directories(destination.parent_path());
			const auto temporary = Destination(output, Path(Text(entry.relative) + ".cachebuilder.tmp"));
			if (asset.sound)
			{
				WriteVSound(asset, temporary);
				++converted;
			}
			else if (entry.type == "mipmap")
			{
				if (FAILED(CreateDDSCache(asset.source, temporary)))
					throw std::runtime_error("DDS generation failed: " + Text(asset.source));
				++converted;
			}
			else { fs::copy_file(asset.source, temporary, fs::copy_options::overwrite_existing); ++copied; }
			if (!asset.sound) fs::last_write_time(temporary, fs::last_write_time(asset.source));
			ReplaceFile(temporary, destination);
			changed.push_back("Resources/" + Text(entry.relative));
		}
		else ++skipped;
		next.emplace(Lower(Text(entry.relative)), entry);
	}
	// 管理対象だったファイルだけを整理
	for (const auto& [key, entry] : previous)
	{
		if (next.contains(key)) continue;
		const auto stale = Destination(output, entry.relative);
		if (!fs::exists(stale)) { changed.push_back("Resources/" + Text(entry.relative)); continue; }
		if (!fs::is_regular_file(stale)) throw std::runtime_error("Stale output is not a file: " + Text(stale));
		const auto extension = Lower(Text(stale.extension()));
		if (extension == ".cso")
		{
			const auto options = settings.Get("Resources/" + Text(entry.relative));
			if (!options.excluded)
			{
				auto retained = entry;
				retained.preload = options.preload;
				next.emplace(key, std::move(retained));
				continue;
			}
		}
		fs::remove(stale);
		changed.push_back("Resources/" + Text(entry.relative));
		++removed;
	}
	WriteText(manifestPath, ManifestText(next));
	// 統合前の実行用設定を整理し、実行時はManifestだけを使う
	const auto legacySettings = Destination(output, "ResourceSettings.ini");
	if (fs::is_regular_file(legacySettings)) fs::remove(legacySettings);
	std::cout << "CacheBuilder: " << assets.size() << " resources, " << converted << " converted, "
		<< copied << " copied, " << skipped << " unchanged, " << removed << " removed\n";
	return changed;
}

std::vector<std::string> CacheBuilder::ListResources(const fs::path& sourceRoot)
{
	std::vector<std::string> result;
	for (const auto& asset : Collect(fs::weakly_canonical(fs::absolute(sourceRoot))))
		result.push_back("Resources/" + Text(asset.entry.relative));
	return result;
}
