#pragma once

#include <cstddef>

namespace ImGuiToolKit
{
inline const char* Utf8(const char* text) { return text; }
inline const char* Utf8(const char8_t* text) { return reinterpret_cast<const char*>(text); }

class Utf8Text
{
public:
	Utf8Text(const char* text) : text(text) {}
	Utf8Text(std::nullptr_t) : text(nullptr) {}
	Utf8Text(const char8_t* text) : text(Utf8(text)) {}
	const char* Get() const { return text; }

private:
	const char* text;
};
}

inline const char* JText(const char* text) { return ImGuiToolKit::Utf8(text); }
inline const char* JText(const char8_t* text) { return ImGuiToolKit::Utf8(text); }
