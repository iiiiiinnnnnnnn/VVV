#include "Resource/MeshCache.h"

#include "Rendering/Core/Graphics.h"

#include <array>
#include <compressapi.h>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace
{
// エラー文字列を設定して失敗を返す
bool FailMeshCacheSave(std::string* error, const std::string& message)
{
	if (error) *error = message;
	return false;
}
} // namespace

// 装備キャッシュを読込み、本体モデルの骨へ接続する
MeshCache::MeshCache(
	const std::filesystem::path& filepath,
	VMDLModel& skeleton,
	const std::string& fallbackNodeName,
	bool retainCpuData)
{
	// 圧縮データを読込
	std::ifstream input(filepath, std::ios::binary);
	if (!input) throw std::runtime_error("MeshCache file not found");
	std::array<char, 8> magic{};
	uint32_t version = 0;
	uint64_t sourceSize = 0;
	uint64_t compressedSize = 0;
	input.read(magic.data(), magic.size());
	input.read(reinterpret_cast<char*>(&version), sizeof(version));
	input.read(reinterpret_cast<char*>(&sourceSize), sizeof(sourceSize));
	input.read(reinterpret_cast<char*>(&compressedSize), sizeof(compressedSize));
	static constexpr std::array<char, 8> expected = {'M', 'E', 'S', 'H', 'C', 'C', 'H', '\0'};
	if (!input || magic != expected || version != 1 || sourceSize == 0 ||
		sourceSize > std::numeric_limits<uint32_t>::max() || compressedSize == 0)
		throw std::runtime_error("Invalid MeshCache header");
	std::vector<uint8_t> compressedData(static_cast<size_t>(compressedSize));
	input.read(reinterpret_cast<char*>(compressedData.data()), compressedData.size());
	if (!input) throw std::runtime_error("MeshCache data is truncated");

	// バイナリを展開
	DECOMPRESSOR_HANDLE decompressor = nullptr;
	if (!CreateDecompressor(COMPRESS_ALGORITHM_XPRESS_HUFF, nullptr, &decompressor))
		throw std::runtime_error("MeshCache decompressor creation failed");
	std::vector<uint8_t> sourceData(static_cast<size_t>(sourceSize));
	SIZE_T writtenSize = 0;
	if (!Decompress(
		decompressor, compressedData.data(), compressedData.size(),
		sourceData.data(), sourceData.size(), &writtenSize) || writtenSize != sourceData.size())
	{
		CloseDecompressor(decompressor);
		throw std::runtime_error("MeshCache decompression failed");
	}
	CloseDecompressor(decompressor);

	// 保存データを復元
	std::vector<std::string> meshNodeNames;
	std::vector<std::vector<std::string>> boneNodeNames;
	std::vector<VMDLModel::MaterialPbrSettings> pbrSettings;
	std::vector<VMDLModel::MaterialVMatSettings> vmatSettings;
	std::istringstream stream(
		std::string(reinterpret_cast<const char*>(sourceData.data()), sourceData.size()),
		std::ios::binary | std::ios::in);
	{
		cereal::BinaryInputArchive archive(stream);
		archive(materials, meshes, meshNodeNames, boneNodeNames, pbrSettings, vmatSettings);
	}
	if (meshes.size() != meshNodeNames.size() || meshes.size() != boneNodeNames.size())
		throw std::runtime_error("MeshCache binding data is invalid");
	for (size_t i = 0; i < materials.size(); ++i)
	{
		if (i < pbrSettings.size())
		{
			materials[i].occlusion = pbrSettings[i].occlusion;
			materials[i].shadowStrength = pbrSettings[i].shadowStrength;
		}
		if (i < vmatSettings.size())
		{
			materials[i].fresnelColor = vmatSettings[i].fresnelColor;
			materials[i].fresnelPower = vmatSettings[i].fresnelPower;
			materials[i].fresnelStrength = vmatSettings[i].fresnelStrength;
			materials[i].isFlatShading = vmatSettings[i].isFlatShading;
		}
	}

	// 本体の骨へ接続
	if (skeleton.GetNodes().empty()) throw std::runtime_error("MeshCache skeleton is empty");
	int fallbackNodeIndex = fallbackNodeName.empty() ? -1 : skeleton.GetNodeIndex(fallbackNodeName.c_str());
	if (!fallbackNodeName.empty() && fallbackNodeIndex < 0)
		throw std::runtime_error("MeshCache fallback node not found: " + fallbackNodeName);
	if (fallbackNodeIndex < 0)
	{
		for (int i = 0; i < static_cast<int>(skeleton.GetNodes().size()); ++i)
		{
			if (skeleton.GetNodes()[i].parentIndex < 0)
			{
				fallbackNodeIndex = i;
				break;
			}
		}
	}
	if (fallbackNodeIndex < 0) fallbackNodeIndex = 0;

	auto* device = Game::Graphics::Instance().GetDevice();
	for (VMDLModel::Material& material : materials)
	{
		skeleton.BuildMaterialTextureResources(device, filepath.parent_path(), material);
		if (!retainCpuData)
		{
			// GPU転送後の埋め込み画像を解放
			material.baseTextureDDS.clear();
			material.normalTextureDDS.clear();
			material.emissiveTextureDDS.clear();
			material.occlusionTextureDDS.clear();
			material.metalnessRoughnessTextureDDS.clear();
			material.baseTextureDDS.shrink_to_fit();
			material.normalTextureDDS.shrink_to_fit();
			material.emissiveTextureDDS.shrink_to_fit();
			material.occlusionTextureDDS.shrink_to_fit();
			material.metalnessRoughnessTextureDDS.shrink_to_fit();
		}
	}
	for (size_t meshIndex = 0; meshIndex < meshes.size(); ++meshIndex)
	{
		VMDLModel::Mesh& mesh = meshes[meshIndex];
		if (mesh.vertices.empty() || mesh.indices.empty() ||
			mesh.vertices.size() > UINT_MAX / sizeof(VMDLModel::Vertex) ||
			mesh.indices.size() > UINT_MAX / sizeof(uint32_t) ||
			mesh.materialIndex < 0 || mesh.materialIndex >= static_cast<int>(materials.size()) ||
			mesh.bones.size() != boneNodeNames[meshIndex].size() || mesh.bones.size() > 256)
			throw std::runtime_error("MeshCache mesh data is invalid");

		mesh.material = &materials[mesh.materialIndex];
		mesh.nodeIndex = skeleton.GetNodeIndex(meshNodeNames[meshIndex].c_str());
		if (mesh.nodeIndex < 0) mesh.nodeIndex = fallbackNodeIndex;
		mesh.node = &skeleton.GetNodes()[mesh.nodeIndex];
		for (size_t boneIndex = 0; boneIndex < mesh.bones.size(); ++boneIndex)
		{
			VMDLModel::Bone& bone = mesh.bones[boneIndex];
			bone.nodeIndex = skeleton.GetNodeIndex(boneNodeNames[meshIndex][boneIndex].c_str());
			if (bone.nodeIndex < 0)
				throw std::runtime_error("MeshCache bone not found: " + boneNodeNames[meshIndex][boneIndex]);
			bone.node = &skeleton.GetNodes()[bone.nodeIndex];
		}

		// GPUバッファを生成
		D3D11_BUFFER_DESC vertexDesc{};
		vertexDesc.ByteWidth = static_cast<UINT>(mesh.vertices.size() * sizeof(VMDLModel::Vertex));
		vertexDesc.Usage = D3D11_USAGE_IMMUTABLE;
		vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		D3D11_SUBRESOURCE_DATA vertexData{};
		vertexData.pSysMem = mesh.vertices.data();
		if (FAILED(device->CreateBuffer(&vertexDesc, &vertexData, mesh.vertexBuffer.GetAddressOf())))
			throw std::runtime_error("MeshCache vertex buffer failed");

		D3D11_BUFFER_DESC indexDesc{};
		indexDesc.ByteWidth = static_cast<UINT>(mesh.indices.size() * sizeof(uint32_t));
		indexDesc.Usage = D3D11_USAGE_IMMUTABLE;
		indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		D3D11_SUBRESOURCE_DATA indexData{};
		indexData.pSysMem = mesh.indices.data();
		if (FAILED(device->CreateBuffer(&indexDesc, &indexData, mesh.indexBuffer.GetAddressOf())))
			throw std::runtime_error("MeshCache index buffer failed");

		mesh.indexCount = static_cast<uint32_t>(mesh.indices.size());
		if (!retainCpuData)
		{
			// GPU転送後のCPUメッシュを解放
			mesh.vertices.clear();
			mesh.indices.clear();
			mesh.vertices.shrink_to_fit();
			mesh.indices.shrink_to_fit();
		}
	}
}

// 指定した複数メッシュを一つの装備キャッシュへ保存する
bool MeshCache::Save(const std::filesystem::path& filepath, const VMDLModel& source,
	const std::vector<int>& meshIndices, std::string* error)
{
	if (error) error->clear();
	if (filepath.empty()) return FailMeshCacheSave(error, "Save path is empty.");
	if (meshIndices.empty()) return FailMeshCacheSave(error, "No meshes are selected.");

	try
	{
		std::vector<VMDLModel::Material> cacheMaterials;
		std::vector<VMDLModel::Mesh> cacheMeshes;
		std::vector<std::string> meshNodeNames;
		std::vector<std::vector<std::string>> boneNodeNames;
		std::vector<VMDLModel::MaterialPbrSettings> pbrSettings;
		std::vector<VMDLModel::MaterialVMatSettings> vmatSettings;
		std::unordered_map<int, int> materialRemap;
		cacheMeshes.reserve(meshIndices.size());
		meshNodeNames.reserve(meshIndices.size());
		boneNodeNames.reserve(meshIndices.size());

		for (int meshIndex : meshIndices)
		{
			if (meshIndex < 0 || meshIndex >= static_cast<int>(source.meshes.size()))
				throw std::runtime_error("Selected mesh index is invalid.");
			const VMDLModel::Mesh& sourceMesh = source.meshes[meshIndex];
			if (sourceMesh.vertices.empty() || sourceMesh.indices.empty())
				throw std::runtime_error("Selected mesh has no CPU vertex data.");
			if (sourceMesh.nodeIndex < 0 ||
				sourceMesh.nodeIndex >= static_cast<int>(source.nodes.size()))
				throw std::runtime_error("Selected mesh node is invalid.");
			if (sourceMesh.materialIndex < 0 ||
				sourceMesh.materialIndex >= static_cast<int>(source.materials.size()))
				throw std::runtime_error("Selected mesh material is invalid.");

			int cacheMaterialIndex = -1;
			const auto foundMaterial = materialRemap.find(sourceMesh.materialIndex);
			if (foundMaterial != materialRemap.end())
			{
				cacheMaterialIndex = foundMaterial->second;
			}
			else
			{
				cacheMaterialIndex = static_cast<int>(cacheMaterials.size());
				materialRemap.emplace(sourceMesh.materialIndex, cacheMaterialIndex);
				const VMDLModel::Material& material = source.materials[sourceMesh.materialIndex];
				cacheMaterials.push_back(material);
				pbrSettings.push_back({material.occlusion, material.shadowStrength});
				vmatSettings.push_back({material.fresnelColor, material.fresnelPower,
					material.fresnelStrength, material.isFlatShading});
			}

			VMDLModel::Mesh mesh = sourceMesh;
			mesh.materialIndex = cacheMaterialIndex;
			mesh.material = nullptr;
			mesh.node = nullptr;
			mesh.vertexBuffer.Reset();
			mesh.indexBuffer.Reset();
			for (VMDLModel::Bone& bone : mesh.bones) bone.node = nullptr;
			cacheMeshes.push_back(std::move(mesh));
			meshNodeNames.push_back(source.nodes[sourceMesh.nodeIndex].name);

			auto& names = boneNodeNames.emplace_back();
			names.reserve(sourceMesh.bones.size());
			for (const VMDLModel::Bone& bone : sourceMesh.bones)
			{
				if (bone.nodeIndex < 0 || bone.nodeIndex >= static_cast<int>(source.nodes.size()))
					throw std::runtime_error("Selected mesh bone is invalid.");
				names.push_back(source.nodes[bone.nodeIndex].name);
			}
		}

		std::ostringstream sourceStream(std::ios::binary | std::ios::out);
		{
			cereal::BinaryOutputArchive archive(sourceStream);
			archive(cacheMaterials, cacheMeshes, meshNodeNames, boneNodeNames, pbrSettings,
				vmatSettings);
		}
		const std::string sourceData = sourceStream.str();
		if (sourceData.empty() || sourceData.size() > std::numeric_limits<uint32_t>::max())
			throw std::runtime_error("MeshCache source data is too large.");

		COMPRESSOR_HANDLE compressor = nullptr;
		if (!CreateCompressor(COMPRESS_ALGORITHM_XPRESS_HUFF, nullptr, &compressor))
			throw std::runtime_error("MeshCache compressor creation failed.");

		SIZE_T compressedSize = 0;
		Compress(compressor, sourceData.data(), sourceData.size(), nullptr, 0, &compressedSize);
		if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || compressedSize == 0)
		{
			CloseCompressor(compressor);
			throw std::runtime_error("MeshCache compressed size calculation failed.");
		}
		std::vector<uint8_t> compressedData(compressedSize);
		if (!Compress(compressor, sourceData.data(), sourceData.size(), compressedData.data(),
				compressedData.size(), &compressedSize))
		{
			CloseCompressor(compressor);
			throw std::runtime_error("MeshCache compression failed.");
		}
		CloseCompressor(compressor);
		compressedData.resize(compressedSize);

		std::ofstream output(filepath, std::ios::binary | std::ios::trunc);
		if (!output) throw std::runtime_error("MeshCache output could not be opened.");
		static constexpr std::array<char, 8> magic = {'M', 'E', 'S', 'H', 'C', 'C', 'H', '\0'};
		const uint32_t version = 1;
		const uint64_t sourceSize = sourceData.size();
		const uint64_t storedCompressedSize = compressedData.size();
		output.write(magic.data(), magic.size());
		output.write(reinterpret_cast<const char*>(&version), sizeof(version));
		output.write(reinterpret_cast<const char*>(&sourceSize), sizeof(sourceSize));
		output.write(
			reinterpret_cast<const char*>(&storedCompressedSize), sizeof(storedCompressedSize));
		output.write(
			reinterpret_cast<const char*>(compressedData.data()), compressedData.size());
		if (!output.good()) throw std::runtime_error("MeshCache output write failed.");
		return true;
	}
	catch (const std::exception& exception)
	{
		return FailMeshCacheSave(error, exception.what());
	}
}
