#pragma once

#include <algorithm>
#include <cctype>
#include <limits>
#include <map>
#include <string>
#include <vector>

#include "Resource/ResourcePath.h"

namespace SoundPaths
{
inline std::string Lower(std::string value)
{
	std::transform(value.begin(), value.end(), value.begin(),
		[](unsigned char character) { return static_cast<char>(std::tolower(character)); });
	return value;
}

inline int Variant(const std::filesystem::path& path)
{
	const std::string stem = ResourcePath::Text(path.stem());
	const auto bracket = stem.rfind('[');
	if (bracket == std::string::npos || stem.back() != ']' || bracket + 2 >= stem.size()) return -1;
	int result = 0;
	for (size_t index = bracket + 1; index + 1 < stem.size(); ++index)
	{
		const char digit = stem[index];
		if (digit < '0' || digit > '9') return -1;
		if (result > (std::numeric_limits<int>::max() - (digit - '0')) / 10) return -1;
		result = result * 10 + digit - '0';
	}
	return result;
}

inline std::filesystem::path Group(std::filesystem::path path)
{
	if (Variant(path) >= 0)
	{
		std::string stem = ResourcePath::Text(path.stem());
		auto filename = ResourcePath::Path(stem.substr(0, stem.rfind('[')));
		filename += path.extension();
		path.replace_filename(filename);
	}
	return path;
}

// WAVの実ファイルから、連番をひとつの再生グループとして集める。
inline std::vector<std::pair<int, std::filesystem::path>> Waves(const std::filesystem::path& group)
{
	std::map<int, std::filesystem::path> result;
	std::error_code error;
	if (!std::filesystem::is_directory(group.parent_path(), error)) return {};
	const std::string groupStem = Lower(ResourcePath::Text(Group(group).stem()));
	for (std::filesystem::directory_iterator iterator(group.parent_path(), error), end;
		!error && iterator != end; iterator.increment(error))
	{
		const auto& file = *iterator;
		if (!file.is_regular_file() || Lower(ResourcePath::Text(file.path().extension())) != ".wav") continue;
		if (Lower(ResourcePath::Text(Group(file.path()).stem())) != groupStem) continue;
		const int variant = std::max(0, Variant(file.path()));
		result.emplace(variant, file.path());
	}
	return {result.begin(), result.end()};
}
}
