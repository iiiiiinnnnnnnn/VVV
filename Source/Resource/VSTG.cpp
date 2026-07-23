// VSTG.cpp

#include "Resource/VSTG.h"

#include <fstream>

#include "Gameplay/Lighting/LightManager.h"
#include "Gameplay/Stage/Component/StageLoader.h"
#include "Gameplay/Stage/Component/Terrain.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace
{
constexpr uint32_t VstgVersion = 1;

json SaveVector3(const Vector3& value)
{
	return {value.x, value.y, value.z};
}

json SaveColor(const Color& value)
{
	return {value.x, value.y, value.z, value.w};
}

Vector3 LoadVector3(const json& value, const Vector3& fallback = Vector3::Zero)
{
	if (!value.is_array() || value.size() < 3) return fallback;
	return {value[0].get<float>(), value[1].get<float>(), value[2].get<float>()};
}

Color LoadColor(const json& value, const Color& fallback = Color(1, 1, 1, 1))
{
	if (!value.is_array() || value.size() < 4) return fallback;
	return {value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>()};
}

json SaveLight(const Light& light)
{
	return {
		{"name", light.GetName()},
		{"active", light.IsActive()},
		{"color", SaveColor(light.GetColor())},
		{"position", SaveVector3(light.transform.position)},
		{"rotation", {light.transform.rotation.x, light.transform.rotation.y, light.transform.rotation.z, light.transform.rotation.w}}
	};
}

void LoadLight(const json& value, Light& light)
{
	light.SetName(value.value("name", light.GetName()));
	light.SetActive(value.value("active", true));
	if (value.contains("color")) light.SetColor(LoadColor(value["color"]));
	if (value.contains("position")) light.transform.position = LoadVector3(value["position"]);
	if (value.contains("rotation") && value["rotation"].is_array() && value["rotation"].size() >= 4)
	{
		light.transform.rotation = {
			value["rotation"][0].get<float>(), value["rotation"][1].get<float>(),
			value["rotation"][2].get<float>(), value["rotation"][3].get<float>()};
	}
	light.transform.Update();
}

template<typename T>
bool Read(std::ifstream& stream, T& value)
{
	return static_cast<bool>(stream.read(reinterpret_cast<char*>(&value), sizeof(value)));
}

template<typename T>
void Write(std::ofstream& stream, const T& value)
{
	stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
}
}

bool VSTG::Load(const std::filesystem::path& path)
{
	std::ifstream stream(path, std::ios::binary);
	if (!stream)
	{
		error = "VSTG not found: " + path.generic_string();
		return false;
	}
	char magic[4]{};
	uint32_t version = 0;
	uint64_t lightingSize = 0;
	uint64_t stageSize = 0;
	uint64_t terrainSize = 0;
	stream.read(magic, sizeof(magic));
	if (!Read(stream, version) || !Read(stream, lightingSize) || !Read(stream, stageSize) || !Read(stream, terrainSize) ||
		std::string(magic, sizeof(magic)) != "VSTG" || version != VstgVersion)
	{
		error = "Invalid VSTG header.";
		return false;
	}
	lightingJson.resize(static_cast<size_t>(lightingSize));
	stageJson.resize(static_cast<size_t>(stageSize));
	terrainDds.resize(static_cast<size_t>(terrainSize));
	stream.read(lightingJson.data(), static_cast<std::streamsize>(lightingSize));
	stream.read(stageJson.data(), static_cast<std::streamsize>(stageSize));
	stream.read(reinterpret_cast<char*>(terrainDds.data()), static_cast<std::streamsize>(terrainSize));
	if (!stream)
	{
		error = "VSTG data is truncated.";
		return false;
	}
	error.clear();
	return true;
}

bool VSTG::Save(const std::filesystem::path& path) const
{
	std::error_code directoryError;
	if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path(), directoryError);
	if (directoryError)
	{
		error = "VSTG directory could not be created.";
		return false;
	}
	std::ofstream stream(path, std::ios::binary);
	if (!stream)
	{
		error = "VSTG could not be opened for writing.";
		return false;
	}
	stream.write("VSTG", 4);
	Write(stream, VstgVersion);
	const uint64_t lightingSize = lightingJson.size();
	const uint64_t stageSize = stageJson.size();
	const uint64_t terrainSize = terrainDds.size();
	Write(stream, lightingSize);
	Write(stream, stageSize);
	Write(stream, terrainSize);
	stream.write(lightingJson.data(), static_cast<std::streamsize>(lightingSize));
	stream.write(stageJson.data(), static_cast<std::streamsize>(stageSize));
	stream.write(reinterpret_cast<const char*>(terrainDds.data()), static_cast<std::streamsize>(terrainSize));
	error = stream ? "" : "VSTG write failed.";
	return static_cast<bool>(stream);
}

bool VSTG::Capture(Terrain& terrain, StageLoader& stageLoader, const LightManager& lights)
{
	if (!terrain.SaveTerrainMemory(terrainDds))
	{
		error = "Terrain could not be encoded.";
		return false;
	}
	stageJson = stageLoader.SaveJsonText();
	lightingJson = BuildLightingJson(lights);
	error.clear();
	return true;
}

bool VSTG::Apply(Terrain& terrain, StageLoader& stageLoader, LightManager& lights) const
{
	if (!terrain.LoadTerrainMemory(terrainDds)) return false;
	stageLoader.LoadJsonText(stageJson);
	return ApplyLightingJson(lightingJson, lights);
}

std::string VSTG::BuildLightingJson(const LightManager& lights) const
{
	json root;
	root["ambient"] = SaveColor(lights.GetAmbientColor());
	root["directional"] = SaveLight(lights.GetDirectionalLight());
	for (const PointLight& light : lights.GetPointLights())
	{
		json value = SaveLight(light);
		value["range"] = light.GetRange();
		root["points"].push_back(std::move(value));
	}
	for (const SpotLight& light : lights.GetSpotLights())
	{
		json value = SaveLight(light);
		value["range"] = light.GetRange();
		value["innerCone"] = light.GetInnerConeAngle();
		value["outerCone"] = light.GetOuterConeAngle();
		root["spots"].push_back(std::move(value));
	}
	for (const AreaLight& light : lights.GetAreaLights())
	{
		json value = SaveLight(light);
		value["range"] = light.GetRange();
		value["width"] = light.GetWidth();
		value["height"] = light.GetHeight();
		root["areas"].push_back(std::move(value));
	}
	return root.dump();
}

bool VSTG::ApplyLightingJson(const std::string& text, LightManager& lights) const
{
	try
	{
		const json root = json::parse(text);
		if (root.contains("ambient")) lights.SetAmbientColor(LoadColor(root["ambient"]));
		if (root.contains("directional")) LoadLight(root["directional"], lights.GetDirectionalLight());
		lights.GetPointLights().clear();
		lights.GetSpotLights().clear();
		lights.GetAreaLights().clear();
		for (const auto& value : root.value("points", json::array()))
		{
			PointLight& light = lights.AddPointLight();
			LoadLight(value, light);
			light.SetRange(value.value("range", 10.0f));
		}
		for (const auto& value : root.value("spots", json::array()))
		{
			SpotLight& light = lights.AddSpotLight();
			LoadLight(value, light);
			light.SetRange(value.value("range", 10.0f));
			light.SetInnerConeAngle(value.value("innerCone", 0.9f));
			light.SetOuterConeAngle(value.value("outerCone", 0.8f));
		}
		for (const auto& value : root.value("areas", json::array()))
		{
			AreaLight& light = lights.AddAreaLight();
			LoadLight(value, light);
			light.SetRange(value.value("range", 10.0f));
			light.SetWidth(value.value("width", 1.0f));
			light.SetHeight(value.value("height", 1.0f));
		}
		return true;
	}
	catch (const json::exception&)
	{
		return false;
	}
}
