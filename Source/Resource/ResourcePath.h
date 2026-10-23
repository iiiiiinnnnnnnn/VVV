#pragma once

#include <filesystem>
#include <string>
#include <string_view>

// UTF-8 filesystem path conversion. Resource names are resolved by ResourceManager.
namespace ResourcePath
{
inline std::filesystem::path Path(std::string_view value)
{
	std::string text(value);
	for (char& character : text)
		if (character == '\\') character = '/';
	return std::filesystem::path(std::u8string(text.begin(), text.end()));
}

inline std::string Text(const std::filesystem::path& path)
{
	const auto text = path.generic_u8string();
	return {text.begin(), text.end()};
}

} // namespace ResourcePath
