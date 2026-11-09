#pragma once

#include <string>

inline std::wstring Utf8ToWide(const std::string& text)
{
	if (text.empty()) return {};
	const int length =
		MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
	if (length <= 0) return std::wstring(text.begin(), text.end());
	std::wstring result(static_cast<size_t>(length), L'\0');
	MultiByteToWideChar(
		CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), length);
	return result;
}

// Convert a string to lowercase
inline std::wstring ToLowerWString(std::wstring text)
{
	std::transform(
		text.begin(),
		text.end(),
		text.begin(),
		[](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });

	return text;
}

// Convert a string to uppercase
inline std::wstring ToUpperWString(std::wstring text)
{
	std::transform(
		text.begin(),
		text.end(),
		text.begin(),
		[](wchar_t c) { return static_cast<wchar_t>(std::towupper(c)); });

	return text;
}

// Convert a string to lowercase
inline std::string ToLowerString(std::string text)
{
	std::transform(
		text.begin(),
		text.end(),
		text.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });

	return text;
}

// Convert a string to uppercase
inline std::string ToUpperString(std::string text)
{
	std::transform(
		text.begin(),
		text.end(),
		text.begin(),
		[](unsigned char c) { return static_cast<char>(std::toupper(c)); });

	return text;
}