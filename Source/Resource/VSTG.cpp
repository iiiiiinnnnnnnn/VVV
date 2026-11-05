#include "Resource/VSTG.h"
#include "Resource/SerialJson.h"
#include "Rendering/Core/Graphics.h"
#include "Resource/ResourceManager.h"
#include "Resource/ResourcePath.h"

#include <array>
#include <fstream>
#include <sstream>

#include "Gameplay/Lighting/LightManager.h"
#include "Gameplay/Stage/Component/StageLoader.h"
#include "Gameplay/Stage/Component/Terrain.h"
#include "Physics/Navigation/NavMeshActor.h"
#include "Core/Foundation/Json.h"


json SaveVector3(const Vector3& value)
{
	return {value.x, value.y, value.z};
}

json SaveColor(const Color& value)
{
	return {value.x, value.y, value.z, value.w};
}

Vector3 LoadVector3(const json& value)
{
	return {value.at(0).get<float>(), value.at(1).get<float>(), value.at(2).get<float>()};
}

Color LoadColor(const json& value)
{
	return {
		value.at(0).get<float>(), value.at(1).get<float>(), value.at(2).get<float>(), value.at(3).get<float>()};
}

json SaveLight(const Light& light)
{
	return {{"name", light.GetName()}, {"active", light.IsActive()},
		{"color", SaveColor(light.GetColor())}, {"intensity", light.GetIntensity()},
		{"position", SaveVector3(light.transform.position)},
		{"rotation", {light.transform.rotation.x, light.transform.rotation.y,
						 light.transform.rotation.z, light.transform.rotation.w}}};
}

void LoadLight(const json& value, Light& light)
{
	light.SetName(value.at("name").get<std::string>());
	light.SetActive(value.at("active").get<bool>());
	light.SetColor(LoadColor(value.at("color")));
	light.SetIntensity(value.at("intensity").get<float>());
	light.transform.position = LoadVector3(value.at("position"));
	const auto& rotation = value.at("rotation");
	light.transform.rotation = {
		rotation.at(0).get<float>(), rotation.at(1).get<float>(),
		rotation.at(2).get<float>(), rotation.at(3).get<float>()
	};
}

template <typename T>
bool Read(std::istream& stream, T& value)
{
	return static_cast<bool>(stream.read(reinterpret_cast<char*>(&value), sizeof(value)));
}

template <typename T>
void Write(std::ofstream& stream, const T& value)
{
	stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

bool VSTG::Load(const std::filesystem::path& path)
{
	try
	{
		const auto bytes = ResourceManager::Instance().LoadFile(ResourceManager::Instance().ResolveResourcePath(
			ResourcePath::Text(path), ResourceManager::ResourceType::Stage));
		if (!bytes) throw std::runtime_error("VSTG not found: " + path.generic_string());
		std::istringstream stream(std::string(bytes->begin(), bytes->end()), std::ios::binary);
		serialDocument.reset();
		lightingJson.clear(); stageJson.clear(); terrainSettingsJson.clear(); navMeshSettingsJson.clear(); terrainDds.clear();
		if (!SerialDocument::IsCurrent(stream)) return LoadGokan(stream);
		serialDocument = std::make_shared<SerialDocument>();
		serialDocument->Load(stream, "VSTG");
		auto& root = serialDocument->root;
		root.understood = true;
		const auto loadJson = [&](const char* name, std::string& text) {
			auto* field = root.Find(SerialNode::FieldId(name));
			if (!field || field->kind == SerialKind::Deleted) return;
			try { text = ReadSerialJson(*field).dump(); field->understood = true; }
			catch (const std::exception& exception) { field->error = exception.what(); }
		};
		loadJson("lighting", lightingJson);
		loadJson("stage", stageJson);
		loadJson("terrainSettings", terrainSettingsJson);
		loadJson("navigationSettings", navMeshSettingsJson);
		SerialInputArchive archive(root);
		archive(CEREAL_NVP(terrainDds));
		error.clear();
		return true;
	}
	catch (const std::exception& exception) { error = exception.what(); return false; }
}

bool VSTG::LoadGokan(std::istream& stream)
{
	char magic[4]{};
	uint32_t version = 0;
	uint64_t lightingSize = 0;
	uint64_t stageSize = 0;
	uint64_t terrainSize = 0;
	uint64_t terrainSettingsSize = 0;
	uint64_t navMeshSettingsSize = 0;
	stream.read(magic, sizeof(magic));
	if (!Read(stream, version) || !Read(stream, lightingSize) || !Read(stream, stageSize) ||
		!Read(stream, terrainSize) || !Read(stream, terrainSettingsSize) ||
		!Read(stream, navMeshSettingsSize) || std::string(magic, sizeof(magic)) != "VSTG" ||
		version != 4)
	{
		error = "Invalid VSTG header.";
		return false;
	}

	const auto payloadStart = stream.tellg();
	stream.seekg(0, std::ios::end);
	const uint64_t fileSize = static_cast<uint64_t>(stream.tellg());
	stream.seekg(payloadStart);
	const uint64_t headerSize =
		sizeof(magic) + sizeof(version) +
		sizeof(lightingSize) + sizeof(stageSize) + sizeof(terrainSize) +
		sizeof(terrainSettingsSize) + sizeof(navMeshSettingsSize);
	if (fileSize < headerSize)
	{
		error = "VSTG file is smaller than its header.";
		return false;
	}

	const uint64_t payloadSize = fileSize - headerSize;
	const std::array<uint64_t, 5> sectionSizes = {
		lightingSize,
		stageSize,
		terrainSize,
		terrainSettingsSize,
		navMeshSettingsSize,
	};
	uint64_t expectedPayloadSize = 0;
	for (const uint64_t sectionSize : sectionSizes)
	{
		const uint64_t remainingSize = payloadSize - expectedPayloadSize;
		if (sectionSize > remainingSize)
		{
			error = "VSTG section sizes exceed file size.";
			return false;
		}
		expectedPayloadSize += sectionSize;
	}
	if (expectedPayloadSize != payloadSize)
	{
		error = "VSTG file size does not match section sizes.";
		return false;
	}

	lightingJson.resize(static_cast<size_t>(lightingSize));
	stageJson.resize(static_cast<size_t>(stageSize));
	terrainDds.resize(static_cast<size_t>(terrainSize));
	terrainSettingsJson.resize(static_cast<size_t>(terrainSettingsSize));
	navMeshSettingsJson.resize(static_cast<size_t>(navMeshSettingsSize));
	stream.read(lightingJson.data(), static_cast<std::streamsize>(lightingSize));
	stream.read(stageJson.data(), static_cast<std::streamsize>(stageSize));
	stream.read(
		reinterpret_cast<char*>(terrainDds.data()), static_cast<std::streamsize>(terrainSize));
	stream.read(terrainSettingsJson.data(), static_cast<std::streamsize>(terrainSettingsSize));
	stream.read(navMeshSettingsJson.data(), static_cast<std::streamsize>(navMeshSettingsSize));
	if (!stream)
	{
		error = "VSTG data is truncated.";
		return false;
	}
	error.clear();
	return true;
}

SerialDocument& VSTG::GetSerialDocument() const
{
	if (!serialDocument) serialDocument = std::make_shared<SerialDocument>();
	else if (serialDocument.use_count() > 1) serialDocument = std::make_shared<SerialDocument>(*serialDocument);
	auto& document = *serialDocument;
	document.type = "VSTG";
	document.root.name = "VSTG";
	document.root.understood = true;
	const auto writeJson = [&](const char* name, const std::string& text) {
		if (!text.empty()) WriteSerialJson(document.root.Field(name), json::parse(text));
	};
	writeJson("lighting", lightingJson);
	writeJson("stage", stageJson);
	writeJson("terrainSettings", terrainSettingsJson);
	writeJson("navigationSettings", navMeshSettingsJson);
	SerialOutputArchive archive(document.root);
	archive(CEREAL_NVP(terrainDds));
	return document;
}

bool VSTG::Save(const std::filesystem::path& path) const
{
	try { GetSerialDocument().Save(path); error.clear(); return true; }
	catch (const std::exception& exception) { error = exception.what(); return false; }
}

bool VSTG::Capture(
	Terrain& terrain, NavMeshActor& navMesh, StageLoader& stageLoader, const LightManager& lights)
{
	if (!terrain.SaveTerrainMemory(terrainDds))
	{
		error = "Terrain could not be encoded.";
		return false;
	}
	stageJson = stageLoader.SaveJsonText();
	lightingJson = BuildLightingJson(lights);
	terrainSettingsJson = terrain.SaveSettingsJson();
	navMeshSettingsJson = navMesh.SaveSettingsJson();
	error.clear();
	return true;
}

bool VSTG::Apply(
	Terrain& terrain, NavMeshActor& navMesh, StageLoader& stageLoader, LightManager& lights) const
{
	if (!terrainSettingsJson.empty() && !terrain.LoadSettingsJson(terrainSettingsJson))
	{
		error = "Terrain settings could not be applied.";
		return false;
	}
	if (!terrainDds.empty() && !terrain.LoadTerrainMemory(terrainDds))
	{
		error = "Terrain data could not be applied.";
		return false;
	}
	if (!navMeshSettingsJson.empty() && !navMesh.LoadSettingsJson(navMeshSettingsJson))
	{
		error = "NavMesh settings could not be applied.";
		return false;
	}
	if (!stageJson.empty()) stageLoader.LoadJsonText(stageJson);
	if (!stageLoader.GetError().empty())
	{
		error = stageLoader.GetError();
		return false;
	}
	if (!lightingJson.empty() && !ApplyLightingJson(lightingJson, lights))
	{
		error = "Lighting settings could not be applied.";
		return false;
	}
	error.clear();
	return true;
}

size_t VSTG::BuildEditorStateHash(const Terrain& terrain, const NavMeshActor& navMesh,
	StageLoader& stageLoader, const LightManager& lights) const
{
	size_t hash = static_cast<size_t>(terrain.GetTerrainDataHash(true));
	const std::hash<std::string> stringHash;
	hash ^= stringHash(terrain.SaveSettingsJson()) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
	hash ^= stringHash(navMesh.SaveSettingsJson()) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
	hash ^= stringHash(stageLoader.SaveJsonText()) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
	hash ^= stringHash(BuildLightingJson(lights)) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
	return hash;
}

bool VSTG::ApplyLighting(LightManager& lights) const
{
	error.clear();
	if (ApplyLightingJson(lightingJson, lights)) return true;
	if (error.empty()) error = "Lighting settings could not be applied.";
	return false;
}

std::string VSTG::BuildLightingJson(const LightManager& lights) const
{
	json root;
	root["ambient"] = SaveColor(lights.GetAmbientColor());
	root["directionals"] = json::array();
	for (const auto& light : lights.GetDirectionalLights())
		if (!light.IsPendingDestroy()) root["directionals"].push_back(SaveLight(light));
	const auto& graphics = Game::Graphics::Instance();
	root["ibl"] = {{"skyMap", graphics.GetSkyMapName()},
		{"skyIntensity", graphics.GetSkyBoxRenderer()->GetIntensity()}};
	for (const PointLight& light : lights.GetPointLights())
	{
		if (light.IsPendingDestroy()) continue;

		json value = SaveLight(light);
		value["range"] = light.GetRange();
		root["points"].push_back(std::move(value));
	}
	for (const SpotLight& light : lights.GetSpotLights())
	{
		if (light.IsPendingDestroy()) continue;

		json value = SaveLight(light);
		value["range"] = light.GetRange();
		value["innerCone"] = light.GetInnerConeAngle();
		value["outerCone"] = light.GetOuterConeAngle();
		root["spots"].push_back(std::move(value));
	}
	for (const AreaLight& light : lights.GetAreaLights())
	{
		if (light.IsPendingDestroy()) continue;

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
		
		{
			auto& graphics = Game::Graphics::Instance();
			const auto& ibl = root.at("ibl");
			const std::string skyMap = ibl.at("skyMap").get<std::string>();
			const float intensity = ibl.at("skyIntensity").get<float>();
			if (skyMap != graphics.GetSkyMapName() && !graphics.LoadSkyMap(skyMap))
			{
				error = "Could not load the VSTG IBL: " + skyMap;
				return false;
			}
			graphics.GetSkyBoxRenderer()->SetIntensity(intensity);
		}
		lights.SetAmbientColor(LoadColor(root.at("ambient")));
		
		const auto& directionals = root.at("directionals");
		const size_t count = directionals.size()
			+ root.value("points", json::array()).size()
			+ root.value("spots", json::array()).size()
			+ root.value("areas", json::array()).size();
		if (count > CbLightData::MaxLights)
		{
			error = "VSTG lighting exceeds the shared limit of 150 lights.";
			return false;
		}
		lights.ClearLights();
		if (directionals.empty()) lights.GetDirectionalLight().Destroy();
		for (size_t i = 0; i < directionals.size(); ++i)
		{
			auto* light = i == 0 ? &lights.GetDirectionalLight() : lights.AddDirectionalLight();
			if (!light) return false;
			LoadLight(directionals[i], *light);
		}

		for (const auto& value : root.value("points", json::array()))
		{
			auto* addedLight = lights.AddPointLight();
			if (!addedLight) return false;
			auto& light = *addedLight;
			LoadLight(value, light);
			light.SetRange(value.at("range").get<float>());
		}
		for (const auto& value : root.value("spots", json::array()))
		{
			auto* addedLight = lights.AddSpotLight();
			if (!addedLight) return false;
			auto& light = *addedLight;
			LoadLight(value, light);
			light.SetRange(value.at("range").get<float>());
			light.SetInnerConeAngle(value.at("innerCone").get<float>());
			light.SetOuterConeAngle(value.at("outerCone").get<float>());
		}
		for (const auto& value : root.value("areas", json::array()))
		{
			auto* addedLight = lights.AddAreaLight();
			if (!addedLight) return false;
			auto& light = *addedLight;
			LoadLight(value, light);
			light.SetRange(value.at("range").get<float>());
			light.SetWidth(value.at("width").get<float>());
			light.SetHeight(value.at("height").get<float>());
		}
		return true;
	}
	catch (const json::exception& exception)
	{
		error = "Invalid VSTG lighting data: " + std::string(exception.what());
		return false;
	}
}
