#pragma once
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include <imgui.h>

#include "Common.h"

using ParamValue = std::variant<bool, short, int, float, Color, Vector2, Vector3, Vector4>;
struct ShaderParam
{
	std::string name;
	ParamValue  value;
};
using ShaderParamList = std::vector<ShaderParam>;
using ShaderParamListWithMaterialName = std::unordered_map<std::string, ShaderParamList>;

struct ParamGUIVisitor
{
	const char* name;
	void operator()(bool& v) { ImGui::Checkbox(name, &v); }
	void operator()(short& v) { int tmp = v; ImGui::DragInt(name, &tmp, 0.01f); v = (short)tmp; }
	void operator()(int& v) { ImGui::DragInt(name, &v, 0.01f); }
	void operator()(float& v) { ImGui::DragFloat(name, &v, 0.01f); }
	void operator()(Color& v) { ImGui::ColorEdit4(name, &v.x); }
	void operator()(Vector2& v) { ImGui::DragFloat2(name, &v.x, 0.01f); }
	void operator()(Vector3& v) { ImGui::DragFloat3(name, &v.x, 0.01f); }
	void operator()(Vector4& v) { ImGui::DragFloat4(name, &v.x, 0.01f); }
};

template<typename T>
static bool HasParam(const ShaderParamList& list, const std::string& name)
{
	for (const ShaderParam& p : list)
	{
		if (p.name == name)
		{
			return std::holds_alternative<T>(p.value);
		}
	}
	return false;
}

template<typename T>
static T GetParam(const ShaderParamList& list, const std::string& name, T defaultValue = {})
{
	for (const ShaderParam& p : list)
	{
		if (p.name == name)
		{
			if (const T* v = std::get_if<T>(&p.value))
			{
				return *v;
			}
		}
	}
	return defaultValue;
}
