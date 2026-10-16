#pragma once

#include <filesystem>
#include <string>
#include <string_view>

// リソース種別ごとのルートと拡張子を補う。既存のフルパス指定も受け付ける。
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

inline std::string Resolve(std::string_view value, const char* directory, const char* extension)
{
	if (value.empty()) return {};
	auto path = Path(value).lexically_normal();
	if (!path.is_absolute() && *path.begin() != "Resources")
		path = std::filesystem::path("Resources") / directory / path;
	if (!path.has_extension()) path += extension;
	return Text(path);
}

inline std::string Short(std::string_view value, const char* directory)
{
	auto path = Path(value).lexically_normal();
	std::string text = Text(path);
	const std::string prefix = std::string("Resources/") + directory + "/";
	if (text.starts_with(prefix)) path = Path(text.substr(prefix.size()));
	const auto extension = path.extension();
	if (extension == ".wav" || extension == ".vsnd" || extension == ".vmdl" || extension == ".vstg")
		path.replace_extension();
	return Text(path);
}

inline std::string Model(std::string_view value) { return Resolve(value, "Model", ".vmdl"); }
inline std::string Stage(std::string_view value) { return Resolve(value, "Stage", ".vstg"); }
inline std::string Sound(std::string_view value) { return Resolve(value, "Sound", ".vsnd"); }
inline std::string Animator(std::string_view value) { return Resolve(value, "Animator", ".animator"); }
inline std::string AI(std::string_view value) { return Resolve(value, "AI", ".lua"); }
}
