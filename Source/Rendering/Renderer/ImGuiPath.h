#pragma once

#include <string>
#include "imgui.h"

namespace ImGuiPath
{
// パスは末尾のファイル名を優先する。表示だけを省略し、実際のパスは変えない。
inline std::string Tail(const std::string& path, float width)
{
	if (ImGui::CalcTextSize(path.c_str()).x <= width) return path;
	const std::string prefix = "...";
	if (width <= ImGui::CalcTextSize(prefix.c_str()).x) return prefix;
	size_t first = 0;
	while (first < path.size())
	{
		++first;
		while (first < path.size() && (static_cast<unsigned char>(path[first]) & 0xc0) == 0x80) ++first;
		const std::string result = prefix + path.substr(first);
		if (ImGui::CalcTextSize(result.c_str()).x <= width) return result;
	}
	return prefix;
}
}
