// CacheSettings.h
#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <windows.h>

// 配布除外と先読みの設定
class CacheSettings
{
public:
	struct Options
	{
		bool excluded = false;
		bool preload = false;
	};

	static std::string Key(const std::string& path)
	{
		auto key = std::filesystem::path(path).lexically_normal().generic_string();
		std::transform(key.begin(), key.end(), key.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return key;
	}

	static bool IsImage(const std::string& path)
	{
		const auto extension = std::filesystem::path(Key(path)).extension().string();
		return extension == ".dds" || extension == ".png" || extension == ".jpg" ||
			extension == ".jpeg" || extension == ".tga" || extension == ".bmp" || extension == ".hdr";
	}

	Options Get(const std::string& path) const
	{
		const auto key = Key(path);
		if (const auto found = entries.find(key); found != entries.end()) return found->second;
		return {false, key.starts_with("resources/image/") && IsImage(key)};
	}

	void Set(const std::string& path, Options options)
	{
		if (options.excluded) options.preload = false;
		entries[Key(path)] = options;
	}

	void Load(const std::filesystem::path& path)
	{
		entries.clear();
		if (!std::filesystem::exists(path)) return;
		std::ifstream input(path);
		if (!input) throw std::runtime_error("Cannot read ResourceSettings.ini");
		std::string line, current;
		while (std::getline(input, line))
		{
			if (line.starts_with("\xef\xbb\xbf")) line.erase(0, 3);
			const auto first = line.find_first_not_of(" \t\r");
			if (first == std::string::npos) continue;
			line = line.substr(first, line.find_last_not_of(" \t\r") - first + 1);
			if (line[0] == ';' || line[0] == '#') continue;
			if (line.front() == '[' && line.back() == ']')
			{
				current = Key(line.substr(1, line.size() - 2));
				if (!current.starts_with("resources/") || current.find_first_of(":*?[]") != std::string::npos)
					throw std::runtime_error("Invalid ResourceSettings.ini path: " + current);
				entries[current] = Get(current);
				continue;
			}
			if (current.empty()) throw std::runtime_error("Missing ResourceSettings.ini resource section");
			if (line == "exclude=0") entries[current].excluded = false;
			else if (line == "exclude=1") entries[current].excluded = true;
			else if (line == "preload=0") entries[current].preload = false;
			else if (line == "preload=1") entries[current].preload = true;
			else throw std::runtime_error("Invalid ResourceSettings.ini option: " + line);
		}
		if (!input.eof()) throw std::runtime_error("ResourceSettings.ini read failed");
		for (auto& [pathKey, options] : entries)
			if (options.excluded) options.preload = false;
	}

	std::string Serialize() const
	{
		std::ostringstream output;
		output << "; CACHE MANAGER\n; Resources/Image images preload by default\n";
		for (const auto& [path, options] : entries)
			output << "\n[" << path << "]\nexclude=" << options.excluded
				<< "\npreload=" << (options.preload && !options.excluded) << '\n';
		return output.str();
	}

	void Save(const std::filesystem::path& path) const
	{
		if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
		const std::filesystem::path temporary = path.wstring() + L".saving.tmp";
		std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
		output << Serialize();
		output.close();
		if (!output || !MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
			throw std::runtime_error("Cannot save ResourceSettings.ini");
	}

private:
	std::map<std::string, Options> entries;
};
