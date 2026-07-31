// VSTG.h

#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "Core\Foundation\Common.h"

class LightManager;
class NavMeshActor;
class StageLoader;
class Terrain;

class VSTG
{
public:
	bool Load(const std::filesystem::path& path);
	bool Save(const std::filesystem::path& path) const;
	bool Capture(
		Terrain& terrain,
		NavMeshActor& navMesh,
		StageLoader& stageLoader,
		const LightManager& lights);
	bool Apply(
		Terrain& terrain,
		NavMeshActor& navMesh,
		StageLoader& stageLoader,
		LightManager& lights) const;

	const std::string& GetError() const { return error; }

private:
	std::string BuildLightingJson(const LightManager& lights) const;
	bool ApplyLightingJson(const std::string& text, LightManager& lights) const;

	std::string lightingJson;
	std::string stageJson;
	std::string terrainSettingsJson;
	std::string navMeshSettingsJson;
	std::vector<uint8_t> terrainDds;
	mutable std::string error;
};
