#include "Audio/SoundTrackRegistry.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <set>
#include <stdexcept>
#include <string_view>
#include <windows.h>

namespace
{
std::string Trim(const std::string& value)
{
	const size_t first = value.find_first_not_of(" \t\r\n");
	if (first == std::string::npos) return {};
	return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}

std::string Upper(std::string value)
{
	std::transform(value.begin(), value.end(), value.begin(),
		[](unsigned char c) { return static_cast<char>(std::toupper(c)); });
	return value;
}

std::string SoundTrackName(const std::string& resourcePath)
{
	std::filesystem::path path(resourcePath);
	path.replace_extension();
	std::string value = path.lexically_normal().generic_string();
	const std::string upper = Upper(value);
	constexpr std::string_view prefix = "RESOURCES/SOUND/";
	if (upper.starts_with(prefix)) value.erase(0, prefix.size());

	std::string name;
	bool separator = false;
	for (const unsigned char character : value)
	{
		if (std::isalnum(character))
		{
			if (separator && !name.empty()) name.push_back('_');
			name.push_back(static_cast<char>(std::toupper(character)));
			separator = false;
		}
		else
		{
			separator = true;
		}
	}
	if (name.empty()) name = "TRACK";
	if (std::isdigit(static_cast<unsigned char>(name.front()))) name.insert(name.begin(), '_');
	return name;
}
}

void SoundTrackRegistry::Load(const std::filesystem::path& path)
{
	entries.clear();
	if (!std::filesystem::exists(path)) return;
	std::ifstream input(path);
	if (!input) throw std::runtime_error("Cannot read sound track registry: " + path.string());

	bool inTracks = false;
	std::string line;
	while (std::getline(input, line))
	{
		if (line.starts_with("\xef\xbb\xbf")) line.erase(0, 3);
		line = Trim(line);
		if (line.empty() || line[0] == ';' || line[0] == '#') continue;
		if (line.front() == '[' && line.back() == ']')
		{
			inTracks = Upper(line.substr(1, line.size() - 2)) == "TRACKS";
			continue;
		}
		if (!inTracks) continue;

		const size_t equal = line.find('=');
		if (equal == std::string::npos)
			throw std::runtime_error("Invalid sound track entry: " + line);
		int track = -1;
		try
		{
			size_t consumed = 0;
			track = std::stoi(Trim(line.substr(0, equal)), &consumed);
			if (consumed != Trim(line.substr(0, equal)).size()) throw std::invalid_argument("track");
		}
		catch (...) { throw std::runtime_error("Invalid sound track number: " + line); }

		Entry entry;
		std::string value = Trim(line.substr(equal + 1));
		const size_t legacySeparator = value.find('|');
		if (legacySeparator != std::string::npos)
			value = Trim(value.substr(legacySeparator + 1));
		entry.path = NormalizeResourcePath(value);
		Set(track, std::move(entry));
	}
	if (!input.eof()) throw std::runtime_error("Sound track registry read failed");
}

void SoundTrackRegistry::Save(const std::filesystem::path& path) const
{
	std::filesystem::create_directories(path.parent_path());
	const std::filesystem::path temporary = path.wstring() + L".tmp";
	std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
	if (!output) throw std::runtime_error("Cannot write sound track registry: " + path.string());
	output << "; SOUND TRACK MANAGER\n[tracks]\n";
	for (const auto& [track, entry] : entries)
		output << track << '=' << entry.path << '\n';
	output.close();
	if (!output) throw std::runtime_error("Sound track registry write failed: " + path.string());
	if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
	{
		DeleteFileW(temporary.c_str());
		throw std::runtime_error("Cannot replace sound track registry: " + path.string());
	}
}

void SoundTrackRegistry::GenerateHeader(const std::filesystem::path& path) const
{
	std::ostringstream generated;
	generated << "#pragma once\n\n";
	generated << "enum class SoundTrack : int\n{\n";
	std::set<std::string> names;
	for (const auto& [track, entry] : entries)
	{
		std::string name = ConstantName(entry.path);
		if (!names.insert(name).second)
			throw std::runtime_error("Duplicate SoundTrack enum name: " + name);
		generated << "\t" << name << " = " << track << ",\n";
	}
	generated << "};\n";
	const std::string content = generated.str();
	if (std::filesystem::exists(path))
	{
		std::ifstream input(path, std::ios::binary);
		const std::string current{
			std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
		if (current == content) return;
	}

	std::filesystem::create_directories(path.parent_path());
	const std::filesystem::path temporary = path.wstring() + L".tmp";
	std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
	if (!output) throw std::runtime_error("Cannot write generated sound track header: " + path.string());
	output.write(content.data(), static_cast<std::streamsize>(content.size()));
	output.close();
	if (!output) throw std::runtime_error("Generated sound track header write failed: " + path.string());
	if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
	{
		DeleteFileW(temporary.c_str());
		throw std::runtime_error("Cannot replace generated sound track header: " + path.string());
	}
}

void SoundTrackRegistry::Set(int track, Entry entry)
{
	if (track < MinimumTrack || track > MaximumTrack)
		throw std::runtime_error("Sound track number must be between 0 and 10000");
	entry.path = NormalizeResourcePath(entry.path);
	if (entry.path.empty()) throw std::runtime_error("Sound track path is empty");
	if (entry.path.find('|') != std::string::npos || entry.path.find_first_of("\r\n") != std::string::npos)
		throw std::runtime_error("Sound track path contains invalid characters");
	entries[track] = std::move(entry);
}

const SoundTrackRegistry::Entry* SoundTrackRegistry::Find(int track) const
{
	const auto found = entries.find(track);
	return found == entries.end() ? nullptr : &found->second;
}

std::string SoundTrackRegistry::NormalizeResourcePath(const std::string& path)
{
	if (path.empty()) return {};
	std::filesystem::path normalizedPath(path);
	if (normalizedPath.is_absolute()) return normalizedPath.lexically_normal().generic_string();
	std::string normalized = normalizedPath.lexically_normal().generic_string();
	std::string lower = normalized;
	std::transform(lower.begin(), lower.end(), lower.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	if (lower == "resources" || lower.starts_with("resources/")) return normalized;
	return (std::filesystem::path("Resources") / normalizedPath).lexically_normal().generic_string();
}

std::string SoundTrackRegistry::ConstantName(const std::string& path)
{
	return SoundTrackName(path);
}
