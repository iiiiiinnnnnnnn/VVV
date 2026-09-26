// MeshCache.h
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "Resource/VMDLModel.h"

// 本体の骨を共有する装備メッシュ
class MeshCache
{
public:
	// 装備キャッシュを読込み、本体モデルの骨へ接続する
	MeshCache(
		const std::filesystem::path& filepath,
		VMDLModel& skeleton,
		const std::string& fallbackNodeName = {},
		bool retainCpuData = false);

	// 指定した複数メッシュを一つの装備キャッシュへ保存する
	static bool Save(const std::filesystem::path& filepath, const VMDLModel& source,
		const std::vector<int>& meshIndices, std::string* error = nullptr);
	void SyncMaterialsFrom(const VMDLModel& source);

	// 装備キャッシュ内の描画メッシュを取得する
	const std::vector<VMDLModel::Mesh>& GetMeshes() const { return meshes; }
	std::vector<VMDLModel::Mesh>& GetMeshes() { return meshes; }

private:
	friend class VMDLModel;

	std::vector<VMDLModel::Material> materials;
	std::vector<VMDLModel::Mesh> meshes;
};
