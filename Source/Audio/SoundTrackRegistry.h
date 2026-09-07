#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

class SoundTrackRegistry
{
public:
	static constexpr int MinimumTrack = 0;
	static constexpr int MaximumTrack = 10000;

	struct Entry
	{
		std::string path;
	};

	void Load(const std::filesystem::path& path);
	void Save(const std::filesystem::path& path) const;
	void GenerateHeader(const std::filesystem::path& path) const;
	void Set(int track, Entry entry);
	void Remove(int track) { entries.erase(track); }
	const std::map<int, Entry>& GetEntries() const { return entries; }
	const Entry* Find(int track) const;

	static std::string NormalizeResourcePath(const std::string& path);
	static std::string ConstantName(const std::string& path);

private:
	std::map<int, Entry> entries;
};
