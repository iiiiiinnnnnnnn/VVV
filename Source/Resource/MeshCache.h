#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "Resource/VMDLModel.h"

// 本体の骨を共有する装備メッシュ
class MeshCache
{
public:
	MeshCache(
		const std::filesystem::path& filepath,
		VMDLModel& skeleton,
		const std::string& fallbackNodeName = {});

	const std::vector<VMDLModel::Mesh>& GetMeshes() const { return meshes; }

private:
	std::vector<VMDLModel::Material> materials;
	std::vector<VMDLModel::Mesh> meshes;
};
