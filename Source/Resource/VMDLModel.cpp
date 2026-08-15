#include "Resource/VMDLModel.h"
#include "Application/SettingsAndDebug/DebugUtil.h"
#include "Resource/GLTFImporter.h"
#include "Resource/GpuResourceUtils.h"
#include "Rendering/Core/Graphics.h"
#include "Core/Foundation/DirectXTexConverts.h"
#include "Core/Foundation/DirectXTexConverts.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <compressapi.h>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>

namespace
{
std::string ToUpperAscii(std::string value)
{
	std::transform(value.begin(), value.end(), value.begin(),
		[](unsigned char c) { return static_cast<char>(std::toupper(c)); });
	return value;
}

std::string NormalizeNodeMatchName(std::string_view value)
{
	std::string normalized;
	normalized.reserve(value.size());
	for (const unsigned char c : value)
	{
		if (c >= 0x80) normalized.push_back(static_cast<char>(c));
		else if (std::isalnum(c)) normalized.push_back(static_cast<char>(std::toupper(c)));
	}
	return normalized;
}

bool HasNodeSide(const std::string& name, bool left)
{
	const std::string upper = ToUpperAscii(name);
	const char* longSide = left ? "LEFT" : "RIGHT";
	const char shortSide = left ? 'L' : 'R';
	if (upper.find(longSide) != std::string::npos) return true;
	const std::array<std::string, 6> patterns = {
		std::string("_") + shortSide + "_",
		std::string(".") + shortSide + ".",
		std::string("-") + shortSide + "-",
		std::string(1, shortSide) + "_",
		std::string(1, shortSide) + ".",
		std::string(1, shortSide) + "-",
	};
	for (const std::string& pattern : patterns)
	{
		if (upper.starts_with(pattern) || upper.find(pattern) != std::string::npos) return true;
	}
	if (upper.ends_with(std::string("_") + shortSide) ||
		upper.ends_with(std::string(".") + shortSide) ||
		upper.ends_with(std::string("-") + shortSide))
		return true;
	return name.find(reinterpret_cast<const char*>(left ? u8"左" : u8"右")) != std::string::npos;
}

bool HasNodeRegion(const std::string& name, bool front)
{
	const std::string normalized = NormalizeNodeMatchName(name);
	if (front)
	{
		return normalized.find("FRONT") != std::string::npos ||
			   normalized.find("FORE") != std::string::npos ||
			   name.find(reinterpret_cast<const char*>(u8"前")) != std::string::npos;
	}
	return normalized.find("BACK") != std::string::npos ||
		   normalized.find("HIND") != std::string::npos ||
		   normalized.find("REAR") != std::string::npos ||
		   name.find(reinterpret_cast<const char*>(u8"後")) != std::string::npos;
}

int FindBestIkNode(const std::vector<VMDLModel::Node>& nodes,
	std::initializer_list<const char*> keywords, int side, int region, int preferredParent,
	int legNumber = 0)
{
	int bestIndex = -1;
	int bestScore = -1;
	for (int nodeIndex = 0; nodeIndex < static_cast<int>(nodes.size()); ++nodeIndex)
	{
		const auto& node = nodes[nodeIndex];
		const std::string normalized = NormalizeNodeMatchName(node.name);
		int score = -1;
		int priority = static_cast<int>(keywords.size());
		for (const char* keyword : keywords)
		{
			const std::string normalizedKeyword = NormalizeNodeMatchName(keyword);
			if (!normalizedKeyword.empty() &&
				normalized.find(normalizedKeyword) != std::string::npos)
				score = std::max(score, priority * 20 + static_cast<int>(normalizedKeyword.size()));
			--priority;
		}
		if (score < 0) continue;

		if (side != 0)
		{
			const bool expectedSide = HasNodeSide(node.name, side < 0);
			const bool oppositeSide = HasNodeSide(node.name, side > 0);
			if (oppositeSide && !expectedSide) continue;
			if (expectedSide) score += 100;
		}
		if (region != 0)
		{
			const bool expectedRegion = HasNodeRegion(node.name, region < 0);
			const bool oppositeRegion = HasNodeRegion(node.name, region > 0);
			if (oppositeRegion && !expectedRegion) continue;
			if (expectedRegion) score += 60;
		}
		if (legNumber > 0)
		{
			const std::string number = std::to_string(legNumber);
			if (normalized.find("LEG" + number) != std::string::npos ||
				normalized.find(number + "LEG") != std::string::npos)
				score += 80;
		}
		if (preferredParent >= 0)
		{
			if (node.parentIndex == preferredParent) score += 120;
			else
			{
				int parentIndex = node.parentIndex;
				while (parentIndex >= 0 && parentIndex < static_cast<int>(nodes.size()) &&
					   parentIndex != preferredParent)
					parentIndex = nodes[parentIndex].parentIndex;
				if (parentIndex == preferredParent) score += 40;
			}
		}
		if (score > bestScore)
		{
			bestScore = score;
			bestIndex = nodeIndex;
		}
	}
	return bestIndex;
}

bool AssignIkNodeReference(
	std::string& target, const std::vector<VMDLModel::Node>& nodes, int nodeIndex)
{
	if (nodeIndex < 0 || nodeIndex >= static_cast<int>(nodes.size())) return false;
	const std::string reference = std::to_string(nodeIndex) + ":" + nodes[nodeIndex].name;
	if (target == reference) return false;
	target = reference;
	return true;
}
} // namespace

void VMDLModel::BuildEmbeddedDDSFromFileOrSRV(ID3D11Device* device,
	const std::filesystem::path& dirpath, const std::string& textureFileName,
	ID3D11ShaderResourceView* srv, std::vector<uint8_t>& outDDS)
{
	outDDS.clear();

	if (!textureFileName.empty())
	{
		std::filesystem::path texturePath = dirpath / textureFileName;

		HRESULT hr = ConvertTextureFileToDDSBytes(texturePath, outDDS);
		if (SUCCEEDED(hr) && !outDDS.empty())
		{
			return;
		}
	}

	if (srv != nullptr)
	{
		HRESULT hr = ConvertSRVToDDSBytes(device, srv, outDDS);
		if (SUCCEEDED(hr) && !outDDS.empty())
		{
			return;
		}
	}

	outDDS.clear();
}

void VMDLModel::BuildMaterialEmbeddedDDS(
	ID3D11Device* device, const std::filesystem::path& dirpath, VMDLModel::Material& material)
{
	BuildEmbeddedDDSFromFileOrSRV(device, dirpath, material.baseTextureFileName,
		material.baseMap.Get(), material.baseTextureDDS);

	BuildEmbeddedDDSFromFileOrSRV(device, dirpath, material.normalTextureFileName,
		material.normalMap.Get(), material.normalTextureDDS);

	BuildEmbeddedDDSFromFileOrSRV(device, dirpath, material.emissiveTextureFileName,
		material.emissiveMap.Get(), material.emissiveTextureDDS);

	BuildEmbeddedDDSFromFileOrSRV(device, dirpath, material.occlusionTextureFileName,
		material.occlusionMap.Get(), material.occlusionTextureDDS);

	BuildEmbeddedDDSFromFileOrSRV(device, dirpath, material.metalnessRoughnessTextureFileName,
		material.metalnessRoughnessMap.Get(), material.metalnessRoughnessTextureDDS);
}

void VMDLModel::CreateSRVFromEmbeddedDDSOrFile(ID3D11Device* device,
	const std::filesystem::path& dirpath, const std::string& textureFileName,
	const std::vector<uint8_t>& embeddedDDS, uint32_t dummyColor,
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv)
{
	if (srv != nullptr)
	{
		return;
	}

	HRESULT hr = S_OK;

	if (!embeddedDDS.empty())
	{
		hr = DirectX::CreateDDSTextureFromMemory(
			device, embeddedDDS.data(), embeddedDDS.size(), nullptr, srv.GetAddressOf());

		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
		return;
	}

	if (textureFileName.empty())
	{
		hr = GpuResourceUtils::CreateDummyTexture(device, dummyColor, srv.GetAddressOf());

		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
		return;
	}

	std::filesystem::path texturePath = dirpath / textureFileName;

	hr = GpuResourceUtils::LoadTexture(device, texturePath.string().c_str(), srv.GetAddressOf());

	_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
}

void VMDLModel::BuildMaterialTextureResources(
	ID3D11Device* device, const std::filesystem::path& dirpath, VMDLModel::Material& material)
{
	CreateSRVFromEmbeddedDDSOrFile(device, dirpath, material.baseTextureFileName,
		material.baseTextureDDS, 0xFFFFFFFF, material.baseMap);

	CreateSRVFromEmbeddedDDSOrFile(device, dirpath, material.normalTextureFileName,
		material.normalTextureDDS, 0xFFFF7F7F, material.normalMap);

	CreateSRVFromEmbeddedDDSOrFile(device, dirpath, material.emissiveTextureFileName,
		material.emissiveTextureDDS, 0xFF000000, material.emissiveMap);

	CreateSRVFromEmbeddedDDSOrFile(device, dirpath, material.occlusionTextureFileName,
		material.occlusionTextureDDS, 0xFFFFFFFF, material.occlusionMap);

	CreateSRVFromEmbeddedDDSOrFile(device, dirpath, material.metalnessRoughnessTextureFileName,
		material.metalnessRoughnessTextureDDS, 0xFF00FF00, material.metalnessRoughnessMap);
}

void VMDLModel::SyncMaterialTextureToSource(size_t materialIndex, MaterialTextureSlot slot)
{
	if (materialIndex >= materials.size() || materialIndex >= sourceMaterials.size()) return;
	const Material& source = materials[materialIndex];
	Material& target = sourceMaterials[materialIndex];
	switch (slot)
	{
	case MaterialTextureSlot::BaseColor:
		target.baseTextureFileName = source.baseTextureFileName;
		target.baseTextureDDS = source.baseTextureDDS;
		break;
	case MaterialTextureSlot::Normal:
		target.normalTextureFileName = source.normalTextureFileName;
		target.normalTextureDDS = source.normalTextureDDS;
		break;
	case MaterialTextureSlot::MetalnessRoughness:
		target.metalnessRoughnessTextureFileName = source.metalnessRoughnessTextureFileName;
		target.metalnessRoughnessTextureDDS = source.metalnessRoughnessTextureDDS;
		break;
	case MaterialTextureSlot::Occlusion:
		target.occlusionTextureFileName = source.occlusionTextureFileName;
		target.occlusionTextureDDS = source.occlusionTextureDDS;
		break;
	case MaterialTextureSlot::Emissive:
		target.emissiveTextureFileName = source.emissiveTextureFileName;
		target.emissiveTextureDDS = source.emissiveTextureDDS;
		break;
	}
}

bool VMDLModel::ReplaceMaterialTexture(
	size_t materialIndex, MaterialTextureSlot slot, const std::filesystem::path& texturePath)
{
	if (materialIndex >= materials.size() || texturePath.empty()) return false;

	Material& material = materials[materialIndex];
	std::string* filename = nullptr;
	std::vector<uint8_t>* embeddedDDS = nullptr;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>* srv = nullptr;
	switch (slot)
	{
	case MaterialTextureSlot::BaseColor:
		filename = &material.baseTextureFileName;
		embeddedDDS = &material.baseTextureDDS;
		srv = &material.baseMap;
		break;
	case MaterialTextureSlot::Normal:
		filename = &material.normalTextureFileName;
		embeddedDDS = &material.normalTextureDDS;
		srv = &material.normalMap;
		break;
	case MaterialTextureSlot::MetalnessRoughness:
		filename = &material.metalnessRoughnessTextureFileName;
		embeddedDDS = &material.metalnessRoughnessTextureDDS;
		srv = &material.metalnessRoughnessMap;
		break;
	case MaterialTextureSlot::Occlusion:
		filename = &material.occlusionTextureFileName;
		embeddedDDS = &material.occlusionTextureDDS;
		srv = &material.occlusionMap;
		break;
	case MaterialTextureSlot::Emissive:
		filename = &material.emissiveTextureFileName;
		embeddedDDS = &material.emissiveTextureDDS;
		srv = &material.emissiveMap;
		break;
	}
	if (!filename || !embeddedDDS || !srv) return false;

	std::vector<uint8_t> convertedDDS;
	if (FAILED(ConvertTextureFileToDDSBytes(texturePath, convertedDDS)) || convertedDDS.empty())
		return false;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> replacement;
	const HRESULT hr = DirectX::CreateDDSTextureFromMemory(Game::Graphics::Instance().GetDevice(),
		convertedDDS.data(), convertedDDS.size(), nullptr, replacement.GetAddressOf());
	if (FAILED(hr)) return false;

	*filename = texturePath.filename().string();
	*embeddedDDS = std::move(convertedDDS);
	*srv = std::move(replacement);
	SyncMaterialTextureToSource(materialIndex, slot);
	return true;
}

bool VMDLModel::ExportMaterialTexture(
	size_t materialIndex, MaterialTextureSlot slot, const std::filesystem::path& savePath)
{
	if (materialIndex >= materials.size() || savePath.empty()) return false;

	std::filesystem::path ext = savePath.extension();
	if (ext != ".dds" && ext != ".png")
	{
		return false;
	}

	Material& material = materials[materialIndex];
	std::vector<uint8_t>* embeddedDDS = nullptr;
	switch (slot)
	{
	case MaterialTextureSlot::BaseColor:
		embeddedDDS = &material.baseTextureDDS;
		break;
	case MaterialTextureSlot::Normal:
		embeddedDDS = &material.normalTextureDDS;
		break;
	case MaterialTextureSlot::MetalnessRoughness:
		embeddedDDS = &material.metalnessRoughnessTextureDDS;
		break;
	case MaterialTextureSlot::Occlusion:
		embeddedDDS = &material.occlusionTextureDDS;
		break;
	case MaterialTextureSlot::Emissive:
		embeddedDDS = &material.emissiveTextureDDS;
		break;
	}

	if (!embeddedDDS) return false;

	if (ext == ".dds")
	{
		std::ofstream file(savePath, std::ios::binary | std::ios::out | std::ios::trunc);

		if (!file)
		{
			return false;
		}

		file.write(reinterpret_cast<const char*>(embeddedDDS->data()),
			static_cast<std::streamsize>(embeddedDDS->size()));
	}
	else if (ext == ".png")
	{
		HRESULT hr = SaveDDSAsPNG(*embeddedDDS, savePath);
		if (FAILED(hr)) return false;
	}

	return true;
}

bool VMDLModel::ClearMaterialTexture(size_t materialIndex, MaterialTextureSlot slot)
{
	if (materialIndex >= materials.size()) return false;

	Material& material = materials[materialIndex];
	std::string* filename = nullptr;
	std::vector<uint8_t>* embeddedDDS = nullptr;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>* srv = nullptr;
	uint32_t dummyColor = 0xFFFFFFFF;
	switch (slot)
	{
	case MaterialTextureSlot::BaseColor:
		filename = &material.baseTextureFileName;
		embeddedDDS = &material.baseTextureDDS;
		srv = &material.baseMap;
		break;
	case MaterialTextureSlot::Normal:
		filename = &material.normalTextureFileName;
		embeddedDDS = &material.normalTextureDDS;
		srv = &material.normalMap;
		dummyColor = 0xFFFF7F7F;
		break;
	case MaterialTextureSlot::MetalnessRoughness:
		filename = &material.metalnessRoughnessTextureFileName;
		embeddedDDS = &material.metalnessRoughnessTextureDDS;
		srv = &material.metalnessRoughnessMap;
		dummyColor = 0xFF00FF00;
		break;
	case MaterialTextureSlot::Occlusion:
		filename = &material.occlusionTextureFileName;
		embeddedDDS = &material.occlusionTextureDDS;
		srv = &material.occlusionMap;
		break;
	case MaterialTextureSlot::Emissive:
		filename = &material.emissiveTextureFileName;
		embeddedDDS = &material.emissiveTextureDDS;
		srv = &material.emissiveMap;
		dummyColor = 0xFF000000;
		break;
	}
	if (!filename || !embeddedDDS || !srv) return false;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> replacement;
	if (FAILED(GpuResourceUtils::CreateDummyTexture(
			Game::Graphics::Instance().GetDevice(), dummyColor, replacement.GetAddressOf())))
		return false;

	filename->clear();
	embeddedDDS->clear();
	*srv = std::move(replacement);
	SyncMaterialTextureToSource(materialIndex, slot);
	return true;
}

bool VMDLModel::ResetMaterialToGLB(size_t materialIndex)
{
	if (materialIndex >= materials.size()) return false;

	const Material* glbMaterial = nullptr;
	for (const Material& source : sourceMaterials)
	{
		if (source.name != materials[materialIndex].name) continue;
		glbMaterial = &source;
		break;
	}
	if (!glbMaterial) return false;

	Material& material = materials[materialIndex];
	material.baseColor = glbMaterial->baseColor;
	material.emissiveColor = glbMaterial->emissiveColor;
	material.metalness = glbMaterial->metalness;
	material.roughness = glbMaterial->roughness;
	material.occlusion = glbMaterial->occlusion;
	material.occlusionStrength = glbMaterial->occlusionStrength;
	material.shadowStrength = glbMaterial->shadowStrength;
	material.alphaCutoff = glbMaterial->alphaCutoff;
	material.alphaMode = glbMaterial->alphaMode;
	material.fresnelColor = glbMaterial->fresnelColor;
	material.fresnelPower = glbMaterial->fresnelPower;
	material.fresnelStrength = glbMaterial->fresnelStrength;
	material.isFlatShading = glbMaterial->isFlatShading;
	return true;
}

VMDLModel::VMDLModel(const char* filename, float sampleRate, const char* savePath)
{
	auto device = Game::Graphics::Instance().GetDevice();

	std::filesystem::path sourceFilepath(filename);
	std::filesystem::path dirpath(sourceFilepath.parent_path());
	std::filesystem::path extension = sourceFilepath.extension();

	std::filesystem::path cerealFilepath;
	if (savePath && savePath[0] != '\0')
	{
		cerealFilepath = savePath;
	}
	else if (extension == ".vmdl")
	{
		cerealFilepath = sourceFilepath;
	}
	modelCacheFilepath = cerealFilepath;

	if (extension == ".vmdl" && std::filesystem::exists(sourceFilepath))
	{
		Deserialize(sourceFilepath.string().c_str());
	}
	else if (extension == ".gltf" || extension == ".glb")
	{
		GLTFImporter importer(filename);

		importer.LoadMaterials(materials, device);

		importer.LoadNodes(nodes);

		importer.LoadMeshes(meshes, nodes);

		importer.LoadAnimations(animations, nodes, sampleRate);

		for (Material& material : materials)
		{
			BuildMaterialEmbeddedDDS(device, dirpath, material);
		}
		sourceMaterials = materials;
	}
	else
	{
		_ASSERT_EXPR_A(false, "found not model file");
	}

	if (extension == ".gltf" || extension == ".glb") ApplyForwardDirectionCorrection();

	for (size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex)
	{
		const int parentIndex = nodes[nodeIndex].parentIndex;
		if (parentIndex >= static_cast<int>(nodes.size()))
			throw std::runtime_error(
				"Invalid parent node index: node=" + std::to_string(nodeIndex) +
				", parent=" + std::to_string(parentIndex));
	}
	for (size_t meshIndex = 0; meshIndex < meshes.size(); ++meshIndex)
	{
		const Mesh& mesh = meshes[meshIndex];
		if (mesh.materialIndex < 0 || mesh.materialIndex >= static_cast<int>(materials.size()))
			throw std::runtime_error("Invalid material index: mesh=" + std::to_string(meshIndex) +
									 ", material=" + std::to_string(mesh.materialIndex) +
									 ", count=" + std::to_string(materials.size()));
		if (mesh.nodeIndex < 0 || mesh.nodeIndex >= static_cast<int>(nodes.size()))
			throw std::runtime_error("Invalid mesh node index: mesh=" + std::to_string(meshIndex) +
									 ", node=" + std::to_string(mesh.nodeIndex) +
									 ", count=" + std::to_string(nodes.size()));
		for (size_t boneIndex = 0; boneIndex < mesh.bones.size(); ++boneIndex)
		{
			const int nodeIndex = mesh.bones[boneIndex].nodeIndex;
			if (nodeIndex < 0 || nodeIndex >= static_cast<int>(nodes.size()))
				throw std::runtime_error(
					"Invalid bone node index: mesh=" + std::to_string(meshIndex) +
					", bone=" + std::to_string(boneIndex) + ", node=" + std::to_string(nodeIndex) +
					", count=" + std::to_string(nodes.size()));
		}
	}
	for (Material& material : materials)
	{
		BuildMaterialTextureResources(device, dirpath, material);
	}

	for (size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex)
	{
		Node& node = nodes.at(nodeIndex);

		node.parent = node.parentIndex >= 0 ? &nodes.at(node.parentIndex) : nullptr;

		if (node.parent != nullptr)
		{
			node.parent->children.emplace_back(&node);
		}
	}

	for (Mesh& mesh : meshes)
	{
		mesh.material = &materials.at(mesh.materialIndex);

		mesh.node = &nodes.at(mesh.nodeIndex);

		{
			D3D11_BUFFER_DESC bufferDesc = {};
			D3D11_SUBRESOURCE_DATA subresourceData = {};

			bufferDesc.ByteWidth = static_cast<UINT>(sizeof(Vertex) * mesh.vertices.size());
			bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
			bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			bufferDesc.CPUAccessFlags = 0;
			bufferDesc.MiscFlags = 0;
			bufferDesc.StructureByteStride = 0;

			subresourceData.pSysMem = mesh.vertices.data();
			subresourceData.SysMemPitch = 0;
			subresourceData.SysMemSlicePitch = 0;

			HRESULT hr = device->CreateBuffer(
				&bufferDesc, &subresourceData, mesh.vertexBuffer.GetAddressOf());

			_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
		}

		{
			D3D11_BUFFER_DESC bufferDesc = {};
			D3D11_SUBRESOURCE_DATA subresourceData = {};

			bufferDesc.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * mesh.indices.size());
			bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
			bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
			bufferDesc.CPUAccessFlags = 0;
			bufferDesc.MiscFlags = 0;
			bufferDesc.StructureByteStride = 0;

			subresourceData.pSysMem = mesh.indices.data();
			subresourceData.SysMemPitch = 0;
			subresourceData.SysMemSlicePitch = 0;

			HRESULT hr = device->CreateBuffer(
				&bufferDesc, &subresourceData, mesh.indexBuffer.GetAddressOf());

			_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
		}
		mesh.indexCount = static_cast<uint32_t>(mesh.indices.size());

		for (Bone& bone : mesh.bones)
		{
			bone.node = &nodes.at(bone.nodeIndex);
		}
	}

	UpdateTransform(Matrix::Identity);
	CaptureRuntimeMorphVisibility();

	// 読込完了後にVMDLを保存
	if (savePath && savePath[0] != '\0' && (extension == ".gltf" || extension == ".glb"))
	{
		Serialize(cerealFilepath.string().c_str());
	}
}

VMDLModel::VMDLModel(const VMDLModel& other)
	: materials(other.materials), sourceMaterials(other.sourceMaterials), meshes(other.meshes),
	  nodes(other.nodes), animations(other.animations), vmdlExtensionData(other.vmdlExtensionData),
	  vmdlIKSettings(other.vmdlIKSettings), vmdlIKPoles(other.vmdlIKPoles),
	  vmdlIKRaySettings(other.vmdlIKRaySettings),
	  vmdlAnimationEditorData(other.vmdlAnimationEditorData),
	  vmdlAnimationControlData(other.vmdlAnimationControlData), vmdlTrailData(other.vmdlTrailData),
	  modelScale(other.modelScale), worldTransform(other.worldTransform),
	  modelCacheFilepath(other.modelCacheFilepath)
{
	RebuildRuntimeReferences();
}

VMDLModel::VMDLModel(VMDLModel&& other) noexcept
	: materials(std::move(other.materials)), sourceMaterials(std::move(other.sourceMaterials)),
	  meshes(std::move(other.meshes)), nodes(std::move(other.nodes)),
	  animations(std::move(other.animations)),
	  vmdlExtensionData(std::move(other.vmdlExtensionData)),
	  vmdlIKSettings(std::move(other.vmdlIKSettings)), vmdlIKPoles(std::move(other.vmdlIKPoles)),
	  vmdlIKRaySettings(std::move(other.vmdlIKRaySettings)),
	  vmdlAnimationEditorData(std::move(other.vmdlAnimationEditorData)),
	  vmdlAnimationControlData(std::move(other.vmdlAnimationControlData)),
	  vmdlTrailData(std::move(other.vmdlTrailData)), modelScale(other.modelScale),
	  worldTransform(other.worldTransform), modelCacheFilepath(std::move(other.modelCacheFilepath))
{
	RebuildRuntimeReferences();
}

VMDLModel& VMDLModel::operator=(const VMDLModel& other)
{
	if (this == &other) return *this;

	materials = other.materials;
	sourceMaterials = other.sourceMaterials;
	meshes = other.meshes;
	nodes = other.nodes;
	animations = other.animations;
	vmdlExtensionData = other.vmdlExtensionData;
	vmdlIKSettings = other.vmdlIKSettings;
	vmdlIKPoles = other.vmdlIKPoles;
	vmdlIKRaySettings = other.vmdlIKRaySettings;
	vmdlAnimationEditorData = other.vmdlAnimationEditorData;
	vmdlAnimationControlData = other.vmdlAnimationControlData;
	vmdlTrailData = other.vmdlTrailData;
	modelScale = other.modelScale;
	worldTransform = other.worldTransform;
	modelCacheFilepath = other.modelCacheFilepath;
	RebuildRuntimeReferences();
	return *this;
}

VMDLModel& VMDLModel::operator=(VMDLModel&& other) noexcept
{
	if (this == &other) return *this;

	materials = std::move(other.materials);
	sourceMaterials = std::move(other.sourceMaterials);
	meshes = std::move(other.meshes);
	nodes = std::move(other.nodes);
	animations = std::move(other.animations);
	vmdlExtensionData = std::move(other.vmdlExtensionData);
	vmdlIKSettings = std::move(other.vmdlIKSettings);
	vmdlIKPoles = std::move(other.vmdlIKPoles);
	vmdlIKRaySettings = std::move(other.vmdlIKRaySettings);
	vmdlAnimationEditorData = std::move(other.vmdlAnimationEditorData);
	vmdlAnimationControlData = std::move(other.vmdlAnimationControlData);
	vmdlTrailData = std::move(other.vmdlTrailData);
	modelScale = other.modelScale;
	worldTransform = other.worldTransform;
	modelCacheFilepath = std::move(other.modelCacheFilepath);
	RebuildRuntimeReferences();
	return *this;
}

void VMDLModel::RebuildRuntimeReferences()
{
	for (Node& node : nodes)
	{
		node.parent = nullptr;
		node.children.clear();
	}

	for (Node& node : nodes)
	{
		if (node.parentIndex < 0) continue;
		if (node.parentIndex >= static_cast<int>(nodes.size())) continue;

		node.parent = &nodes[node.parentIndex];
		node.parent->children.emplace_back(&node);
	}

	for (Mesh& mesh : meshes)
	{
		mesh.material = nullptr;
		mesh.node = nullptr;
		if (mesh.materialIndex >= 0 && mesh.materialIndex < static_cast<int>(materials.size()))
			mesh.material = &materials[mesh.materialIndex];
		if (mesh.nodeIndex >= 0 && mesh.nodeIndex < static_cast<int>(nodes.size()))
			mesh.node = &nodes[mesh.nodeIndex];

		for (Bone& bone : mesh.bones)
		{
			bone.node = nullptr;
			if (bone.nodeIndex >= 0 && bone.nodeIndex < static_cast<int>(nodes.size()))
				bone.node = &nodes[bone.nodeIndex];
		}
	}

	UpdateTransform(Matrix::Identity);
	CaptureRuntimeMorphVisibility();
}

std::shared_ptr<VMDLModel> VMDLModel::Clone() const
{
	return std::make_shared<VMDLModel>(*this);
}

bool VMDLModel::HasSkeleton() const
{
	for (const Mesh& mesh : meshes)
		if (!mesh.bones.empty()) return true;

	return false;
}

void VMDLModel::AppendAnimations(const char* filename)
{
	std::filesystem::path filepath(filename);

	if (filepath.extension() == ".gltf" || filepath.extension() == ".glb")
	{
		GLTFImporter importer(filename);

		std::vector<Node> animNodes;
		importer.LoadNodes(animNodes);

		std::vector<Animation> newAnims;
		importer.LoadAnimations(newAnims, animNodes);

		std::unordered_map<std::string, int> modelNodeMap;
		for (int i = 0; i < (int)nodes.size(); ++i) modelNodeMap[nodes[i].name] = i;

		for (Animation& anim : newAnims)
		{
			Animation remapped;
			//remapped.name = anim.name;
			remapped.name = filepath.stem().string();
			remapped.secondsLength = anim.secondsLength;
			remapped.nodeAnims.resize(nodes.size());

			for (int i = 0; i < (int)nodes.size(); ++i)
			{
				VMDLModel::VectorKeyframe pk;
				pk.seconds = 0.0f;
				pk.value = nodes[i].position;
				remapped.nodeAnims[i].positionKeyframes.push_back(pk);
				pk.seconds = anim.secondsLength;
				remapped.nodeAnims[i].positionKeyframes.push_back(pk);

				VMDLModel::QuaternionKeyframe rk;
				rk.seconds = 0.0f;
				rk.value = nodes[i].rotation;
				remapped.nodeAnims[i].rotationKeyframes.push_back(rk);
				rk.seconds = anim.secondsLength;
				remapped.nodeAnims[i].rotationKeyframes.push_back(rk);

				VMDLModel::VectorKeyframe sk;
				sk.seconds = 0.0f;
				sk.value = nodes[i].scale;
				remapped.nodeAnims[i].scaleKeyframes.push_back(sk);
				sk.seconds = anim.secondsLength;
				remapped.nodeAnims[i].scaleKeyframes.push_back(sk);
			}

			for (int animIdx = 0; animIdx < (int)animNodes.size(); ++animIdx)
			{
				auto it = modelNodeMap.find(animNodes[animIdx].name);
				if (it != modelNodeMap.end())
				{
					remapped.nodeAnims[it->second] = anim.nodeAnims[animIdx];
				}
			}
			animations.push_back(std::move(remapped));
		}
	}
	else
	{
		_ASSERT_EXPR_A(false, "found not model file");
	}
}

int VMDLModel::GetAnimationIndex(const char* name) const
{
	for (size_t animationIndex = 0; animationIndex < animations.size(); ++animationIndex)
	{
		if (animations.at(animationIndex).name == name)
		{
			return static_cast<int>(animationIndex);
		}
	}
	const std::string message = "animation not found: " + std::string(name);
	_ASSERT_EXPR_A(false, message.c_str());
	return -1;
}

static void UpdateNodeTransform(
	VMDLModel::Node& node, const Matrix& parentGlobal, const Matrix& worldTransform)
{
	Matrix S = Matrix::CreateScale(node.scale);
	Matrix R = Matrix::CreateFromQuaternion(node.rotation);
	Matrix T = Matrix::CreateTranslation(node.position);
	Matrix localTransform = S * R * T;
	Matrix globalTransform = localTransform * parentGlobal;

	node.localTransform = localTransform;
	node.globalTransform = globalTransform;
	node.worldTransform = globalTransform * worldTransform;

	for (VMDLModel::Node* child : node.children)
	{
		UpdateNodeTransform(*child, globalTransform, worldTransform);
	}
}

int VMDLModel::GetNodeIndex(const char* name) const
{
	if (!name) return -1;

	const char* separator = strchr(name, ':');
	if (separator && separator != name)
	{
		int referencedIndex = -1;
		const auto result = std::from_chars(name, separator, referencedIndex);
		if (result.ec == std::errc() && result.ptr == separator && referencedIndex >= 0 &&
			referencedIndex < static_cast<int>(nodes.size()) &&
			nodes[referencedIndex].name == separator + 1)
		{
			return referencedIndex;
		}
	}

	for (size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex)
	{
		if (nodes.at(nodeIndex).name == name)
		{
			return static_cast<int>(nodeIndex);
		}
	}
	return -1;
}

int VMDLModel::GetMorphIndex(const char* name) const
{
	if (!name) return -1;
	const std::string normalizedName = ToUpperAscii(name);
	const auto& morphs = vmdlExtensionData.morphs;
	for (int i = 0; i < static_cast<int>(morphs.size()); ++i)
	{
		if (ToUpperAscii(morphs[i].name) == normalizedName) return i;
	}
	return -1;
}

void VMDLModel::NormalizeMorphNames()
{
	auto& morphs = vmdlExtensionData.morphs;
	std::vector<VmdlMorph> normalizedMorphs;
	normalizedMorphs.reserve(morphs.size());
	std::vector<int> indexMap(morphs.size(), -1);
	std::unordered_map<std::string, int> nameToIndex;

	for (int oldIndex = 0; oldIndex < static_cast<int>(morphs.size()); ++oldIndex)
	{
		VmdlMorph& morph = morphs[oldIndex];
		morph.name = ToUpperAscii(morph.name);
		if (morph.name.empty()) morph.name = "MORPH";

		const auto found = nameToIndex.find(morph.name);
		if (found != nameToIndex.end())
		{
			indexMap[oldIndex] = found->second;
			continue;
		}

		const int newIndex = static_cast<int>(normalizedMorphs.size());
		indexMap[oldIndex] = newIndex;
		nameToIndex.emplace(morph.name, newIndex);
		normalizedMorphs.push_back(std::move(morph));
	}

	for (auto& track : vmdlAnimationControlData.morphTracks)
	{
		for (auto& key : track.keys)
		{
			if (key.morphIndex < 0 || key.morphIndex >= static_cast<int>(indexMap.size()))
			{
				key.morphIndex = -1;
				continue;
			}
			key.morphIndex = indexMap[key.morphIndex];
		}
		std::erase_if(track.keys, [](const VmdlMorphKeyframe& key) { return key.morphIndex < 0; });
	}

	morphs = std::move(normalizedMorphs);
}

void VMDLModel::NormalizeAttachmentNames()
{
	for (VmdlRigidBody& rigidBody : vmdlExtensionData.rigidBodies)
	{
		rigidBody.name = ToUpperAscii(rigidBody.name);
		if (rigidBody.name.empty()) rigidBody.name = "RIGIDBODY";
	}
	for (VmdlCollider& collider : vmdlExtensionData.colliders)
	{
		collider.name = ToUpperAscii(collider.name);
		if (collider.name.empty()) collider.name = "COLLIDER";
	}
	for (VmdlSpring& spring : vmdlExtensionData.springs)
	{
		spring.name = ToUpperAscii(spring.name);
		if (spring.name.empty()) spring.name = "SPRING";
	}
	for (VmdlSpringCollider& springCollider : vmdlExtensionData.springColliders)
	{
		springCollider.name = ToUpperAscii(springCollider.name);
		if (springCollider.name.empty()) springCollider.name = "SPRING COLLIDER";
	}
	for (VmdlTrail& trail : vmdlTrailData.trails)
	{
		trail.name = ToUpperAscii(trail.name);
		if (trail.name.empty()) trail.name = "TRAIL";
	}
}

bool VMDLModel::ApplyMorph(const char* name)
{
	return ApplyMorph(GetMorphIndex(name));
}

void VMDLModel::ApplyInitialMorphs()
{
	for (int i = 0; i < static_cast<int>(vmdlExtensionData.morphs.size()); ++i)
	{
		if (vmdlExtensionData.morphs[i].applyOnInitialize) ApplyMorph(i);
	}
}

bool VMDLModel::ApplyMorph(int morphIndex)
{
	const auto& morphs = vmdlExtensionData.morphs;
	if (morphIndex < 0 || morphIndex >= static_cast<int>(morphs.size())) return false;
	if (runtimeMorphVisibility.size() != meshes.size()) CaptureRuntimeMorphVisibility();

	const auto& visibility = morphs[morphIndex].meshVisibility;
	const size_t count = std::min(meshes.size(), visibility.size());
	for (size_t i = 0; i < count; ++i)
	{
		if (visibility[i] == 1) runtimeMorphVisibility[i] = 1;
		else if (visibility[i] == 0) runtimeMorphVisibility[i] = 0;
	}
	return ApplyMorphToMeshes(morphIndex);
}

void VMDLModel::UpdateTransform(const Matrix& worldTransform)
{
	this->worldTransform = worldTransform;

	for (Node& node : nodes)
	{
		if (node.parent == nullptr)
		{
			UpdateNodeTransform(node, Matrix::Identity, worldTransform);
		}
	}
}

Matrix VMDLModel::GetRenderScaleTransform() const
{
	if (modelScale == 1.0f) return Matrix::Identity;

	const Vector3 pivot = worldTransform.Translation();

	return Matrix::CreateTranslation(-pivot) * Matrix::CreateScale(modelScale) *
		   Matrix::CreateTranslation(pivot);
}

Matrix VMDLModel::GetScaledAttachmentTransform(const Matrix& unscaledWorldTransform) const
{
	return unscaledWorldTransform * GetRenderScaleTransform();
}

Vector3 VMDLModel::GetScaledAttachmentVector(const Vector3& unscaledValue) const
{
	return unscaledValue * modelScale;
}

Vector3 VMDLModel::GetUnscaledAttachmentVector(const Vector3& scaledValue) const
{
	return scaledValue / modelScale;
}

void VMDLModel::SetModelScale(float value)
{
	modelScale = std::isfinite(value) ? std::clamp(value, 0.0001f, 10000.0f) : 1.0f;
}

const Matrix& VMDLModel::GetWorldTransform() const
{
	return worldTransform;
}

void VMDLModel::ComputeAnimation(
	int animationIndex, int nodeIndex, float time, NodePose& nodePose) const
{
	const Animation& animation = animations.at(animationIndex);
	const NodeAnim& nodeAnim = animation.nodeAnims.at(nodeIndex);

	if (!nodeAnim.positionKeyframes.empty())
	{
		nodePose.position = nodeAnim.positionKeyframes.back().value;
		for (size_t index = 1; index < nodeAnim.positionKeyframes.size(); ++index)
		{
			const VectorKeyframe& previous = nodeAnim.positionKeyframes[index - 1];
			const VectorKeyframe& next = nodeAnim.positionKeyframes[index];
			if (time > next.seconds) continue;
			const float duration = next.seconds - previous.seconds;
			const float rate = duration > 0.00001f ? (time - previous.seconds) / duration : 0.0f;
			nodePose.position = time <= previous.seconds
									? previous.value
									: Vector3::Lerp(previous.value, next.value, rate);
			break;
		}
	}
	if (!nodeAnim.rotationKeyframes.empty())
	{
		nodePose.rotation = nodeAnim.rotationKeyframes.back().value;
		for (size_t index = 1; index < nodeAnim.rotationKeyframes.size(); ++index)
		{
			const QuaternionKeyframe& previous = nodeAnim.rotationKeyframes[index - 1];
			const QuaternionKeyframe& next = nodeAnim.rotationKeyframes[index];
			if (time > next.seconds) continue;
			const float duration = next.seconds - previous.seconds;
			const float rate = duration > 0.00001f ? (time - previous.seconds) / duration : 0.0f;
			nodePose.rotation = time <= previous.seconds
									? previous.value
									: Quaternion::Slerp(previous.value, next.value, rate);
			break;
		}
	}
	if (!nodeAnim.scaleKeyframes.empty())
	{
		nodePose.scale = nodeAnim.scaleKeyframes.back().value;
		for (size_t index = 1; index < nodeAnim.scaleKeyframes.size(); ++index)
		{
			const VectorKeyframe& previous = nodeAnim.scaleKeyframes[index - 1];
			const VectorKeyframe& next = nodeAnim.scaleKeyframes[index];
			if (time > next.seconds) continue;
			const float duration = next.seconds - previous.seconds;
			const float rate = duration > 0.00001f ? (time - previous.seconds) / duration : 0.0f;
			nodePose.scale = time <= previous.seconds
								 ? previous.value
								 : Vector3::Lerp(previous.value, next.value, rate);
			break;
		}
	}
}

void VMDLModel::ComputeAnimation(
	int animationIndex, float time, std::vector<NodePose>& nodePoses) const
{
	if (nodePoses.size() != nodes.size())
	{
		nodePoses.resize(nodes.size());
	}
	for (size_t nodeIndex = 0; nodeIndex < nodePoses.size(); ++nodeIndex)
	{
		ComputeAnimation(
			animationIndex, static_cast<int>(nodeIndex), time, nodePoses.at(nodeIndex));
	}
}

void VMDLModel::ResetVmdlIKLegsForType()
{
	auto& settings = vmdlIKSettings;
	settings.legs.clear();
	vmdlIKPoles.clear();
	vmdlIKRaySettings.clear();

	int legCount = 0;
	if (settings.type == 1) legCount = 2;
	else if (settings.type == 2) legCount = 4;
	else if (settings.type == 3) legCount = 8;
	settings.legs.resize(legCount);
	vmdlIKPoles.resize(legCount);
	NormalizeVmdlIKRaySettings();

	constexpr const char* humanNames[] = {"Left", "Right"};
	constexpr const char* quadrupedNames[] = {
		"Front Left", "Front Right", "Back Left", "Back Right"};
	for (int i = 0; i < legCount; ++i)
	{
		if (settings.type == 1) settings.legs[i].name = humanNames[i];
		else if (settings.type == 2) settings.legs[i].name = quadrupedNames[i];
		else settings.legs[i].name = "Leg " + std::to_string(i + 1);
	}
	AutoAssignVmdlIKNodes();
}

void VMDLModel::NormalizeVmdlIKRaySettings()
{
	const size_t previousSize = vmdlIKRaySettings.size();
	vmdlIKRaySettings.resize(vmdlIKSettings.legs.size());
	const float defaultStartHeight = vmdlIKSettings.type == 1 ? 0.2f : 1.0f;
	const float defaultLength = vmdlIKSettings.type == 1 ? 0.7f : 6.0f;
	for (size_t index = 0; index < vmdlIKRaySettings.size(); ++index)
	{
		auto& ray = vmdlIKRaySettings[index];
		if (index >= previousSize)
		{
			ray.startOffset = Vector3(0.0f, defaultStartHeight, 0.0f);
			ray.length = defaultLength;
		}
		ray.length = std::max(0.01f, ray.length);
	}
}

bool VMDLModel::AutoAssignVmdlIKNodes()
{
	if (vmdlIKSettings.type == 0 || nodes.empty()) return false;

	bool changed = false;
	int centerIndex = -1;
	if (vmdlIKSettings.type == 1)
		centerIndex = FindBestIkNode(nodes,
			{"HIPS", "PELVIS", "WAIST", "HIP", "ROOT", (const char*)u8"骨盤", (const char*)u8"腰"},
			0, 0, -1);
	else if (vmdlIKSettings.type == 2)
		centerIndex = FindBestIkNode(nodes,
			{"BODY", "SPINE", "CHEST", "PELVIS", "HIPS", "ROOT", (const char*)u8"胴体",
				(const char*)u8"背骨"},
			0, 0, -1);
	else
		centerIndex = FindBestIkNode(nodes,
			{"THORAX", "BODY", "CHEST", "ROOT", (const char*)u8"胸部", (const char*)u8"胴体"}, 0, 0,
			-1);
	if (centerIndex >= 0)
		changed |= AssignIkNodeReference(vmdlIKSettings.centerNode, nodes, centerIndex);
	else centerIndex = GetNodeIndex(vmdlIKSettings.centerNode.c_str());

	for (int i = 0; i < static_cast<int>(vmdlIKSettings.legs.size()); ++i)
	{
		auto& leg = vmdlIKSettings.legs[i];
		const int side = i % 2 == 0 ? -1 : 1;
		const int region = vmdlIKSettings.type == 2 ? (i < 2 ? -1 : 1) : 0;
		const int legNumber = vmdlIKSettings.type == 3 ? i + 1 : 0;

		int rootIndex = -1;
		int midIndex = -1;
		int tipIndex = -1;
		int contactIndex = -1;
		if (vmdlIKSettings.type == 1)
		{
			rootIndex = FindBestIkNode(nodes,
				{"UPPERLEG", "UPLEG", "THIGH", (const char*)u8"太もも", (const char*)u8"腿",
					(const char*)u8"足"},
				side, 0, centerIndex);
			midIndex = FindBestIkNode(nodes,
				{"LOWERLEG", "CALF", "SHIN", "KNEE", (const char*)u8"ひざ", (const char*)u8"膝",
					(const char*)u8"すね"},
				side, 0, rootIndex);
			tipIndex = FindBestIkNode(nodes,
				{"FOOT", "ANKLE", (const char*)u8"足首", (const char*)u8"足"}, side, 0, midIndex);
			contactIndex = FindBestIkNode(nodes,
				{"TOEBASE", "TOE", "BALL", (const char*)u8"つま先", (const char*)u8"爪先"}, side, 0,
				tipIndex);
		}
		else if (vmdlIKSettings.type == 2)
		{
			if (region < 0)
			{
				rootIndex = FindBestIkNode(nodes,
					{"UPPERARM", "SHOULDER", "FRONTLEG", "FORELEG", "ARM", "LEG"}, side, region,
					centerIndex);
				midIndex = FindBestIkNode(nodes,
					{"LOWERARM", "FOREARM", "ELBOW", "LOWERLEG", "KNEE"}, side, region, rootIndex);
				tipIndex = FindBestIkNode(
					nodes, {"HAND", "FRONTFOOT", "PAW", "FOOT", "WRIST"}, side, region, midIndex);
			}
			else
			{
				rootIndex = FindBestIkNode(nodes,
					{"UPPERLEG", "THIGH", "HINDLEG", "BACKLEG", "LEG"}, side, region, centerIndex);
				midIndex = FindBestIkNode(
					nodes, {"LOWERLEG", "CALF", "SHIN", "KNEE"}, side, region, rootIndex);
				tipIndex =
					FindBestIkNode(nodes, {"FOOT", "PAW", "ANKLE", "HOOF"}, side, region, midIndex);
			}
			contactIndex =
				FindBestIkNode(nodes, {"TOE", "CLAW", "HOOF", "BALL"}, side, region, tipIndex);
		}
		else
		{
			rootIndex =
				FindBestIkNode(nodes, {"COXA", "LEGROOT", "LEG"}, 0, 0, centerIndex, legNumber);
			midIndex =
				FindBestIkNode(nodes, {"FEMUR", "LEGMID", "LEG"}, 0, 0, rootIndex, legNumber);
			tipIndex = FindBestIkNode(nodes, {"TIBIA", "LEGTIP", "LEG"}, 0, 0, midIndex, legNumber);
			contactIndex =
				FindBestIkNode(nodes, {"TARSUS", "FOOT", "TOE"}, 0, 0, tipIndex, legNumber);
		}

		changed |= AssignIkNodeReference(leg.root, nodes, rootIndex);
		changed |= AssignIkNodeReference(leg.mid, nodes, midIndex);
		changed |= AssignIkNodeReference(leg.tip, nodes, tipIndex);
		changed |= AssignIkNodeReference(leg.contact, nodes, contactIndex);
	}
	return changed;
}

float VMDLModel::EvaluateFootIKWeight(int animationIndex, float time, int footIndex) const
{
	if (animationIndex < 0) return 0.0f;
	if (animationIndex >= static_cast<int>(animations.size())) return 0.0f;

	const Animation& animation = animations[animationIndex];
	if (animation.secondsLength > 0.0f)
	{
		while (time < 0.0f) time += animation.secondsLength;
		while (time > animation.secondsLength) time -= animation.secondsLength;
	}

	const VmdlFootWeightTrack* selectedTrack = FindFootWeightTrack(animation.name, footIndex);
	if (selectedTrack && !selectedTrack->weights.empty())
	{
		const auto& track = *selectedTrack;
		const float sample = animation.secondsLength > 0.00001f
								 ? std::clamp(time / animation.secondsLength, 0.0f, 1.0f) *
									   static_cast<float>(track.weights.size() - 1)
								 : 0.0f;
		const size_t index0 = static_cast<size_t>(sample);
		const size_t index1 = std::min(index0 + 1, track.weights.size() - 1);
		return std::clamp(std::lerp(track.weights[index0], track.weights[index1],
							  sample - static_cast<float>(index0)),
			0.0f, 1.0f);
	}
	return 0.0f;
}

std::string VMDLModel::MakeFootWeightTrackKey(const std::string& animationName, int footIndex)
{
	return animationName + "::FootWeight:" + std::to_string(footIndex);
}

VMDLModel::VmdlFootWeightTrack* VMDLModel::FindFootWeightTrack(
	const std::string& animationName, int footIndex)
{
	const std::string key = MakeFootWeightTrackKey(animationName, footIndex);
	for (auto& track : vmdlAnimationEditorData.footWeightTracks)
	{
		if (track.animationName == key) return &track;
	}
	return nullptr;
}

const VMDLModel::VmdlFootWeightTrack* VMDLModel::FindFootWeightTrack(
	const std::string& animationName, int footIndex) const
{
	const std::string key = MakeFootWeightTrackKey(animationName, footIndex);
	for (const auto& track : vmdlAnimationEditorData.footWeightTracks)
	{
		if (track.animationName == key) return &track;
	}
	return nullptr;
}

VMDLModel::VmdlFootWeightTrack& VMDLModel::GetOrCreateFootWeightTrack(
	const std::string& animationName, int footIndex)
{
	VmdlFootWeightTrack* track = FindFootWeightTrack(animationName, footIndex);
	if (!track)
	{
		track = &vmdlAnimationEditorData.footWeightTracks.emplace_back();
		track->animationName = MakeFootWeightTrackKey(animationName, footIndex);
	}
	if (track->sampleRate >= VmdlFootWeightTrack::DefaultSampleRate || track->weights.empty())
		return *track;

	float length = 0.0f;
	for (const Animation& animation : animations)
	{
		if (animation.name != animationName) continue;
		length = animation.secondsLength;
		break;
	}
	if (length <= 0.0f) return *track;

	const int sampleCount = std::max(
		2, static_cast<int>(std::ceil(length * VmdlFootWeightTrack::DefaultSampleRate)) + 1);
	std::vector<float> weights(sampleCount);
	for (int i = 0; i < sampleCount; ++i)
	{
		const float sample = static_cast<float>(i) / static_cast<float>(sampleCount - 1) *
							 static_cast<float>(track->weights.size() - 1);
		const size_t index0 = static_cast<size_t>(sample);
		const size_t index1 = std::min(index0 + 1, track->weights.size() - 1);
		weights[i] = std::lerp(
			track->weights[index0], track->weights[index1], sample - static_cast<float>(index0));
	}
	track->sampleRate = VmdlFootWeightTrack::DefaultSampleRate;
	track->weights = std::move(weights);
	return *track;
}

bool VMDLModel::GetColliderInitialActive(int colliderIndex) const
{
	if (colliderIndex < 0) return true;
	if (colliderIndex >= static_cast<int>(vmdlAnimationControlData.colliderInitialActive.size()))
		return true;
	return vmdlAnimationControlData.colliderInitialActive[colliderIndex] != 0;
}

void VMDLModel::SetColliderInitialActive(int colliderIndex, bool active)
{
	if (colliderIndex < 0) return;
	auto& values = vmdlAnimationControlData.colliderInitialActive;
	if (values.size() <= static_cast<size_t>(colliderIndex)) values.resize(colliderIndex + 1, 1);
	values[colliderIndex] = active ? 1 : 0;
}

bool VMDLModel::EvaluateColliderActive(int animationIndex, float time, int colliderIndex) const
{
	bool active = GetColliderInitialActive(colliderIndex);
	if (animationIndex < 0 || animationIndex >= static_cast<int>(animations.size())) return active;

	const Animation& animation = animations[animationIndex];
	if (animation.secondsLength > 0.0f)
	{
		while (time < 0.0f) time += animation.secondsLength;
		while (time > animation.secondsLength) time -= animation.secondsLength;
	}

	for (const auto& track : vmdlAnimationControlData.colliderTracks)
	{
		if (track.animationName != animation.name || track.colliderIndex != colliderIndex) continue;
		for (const auto& key : track.keys)
		{
			if (key.seconds > time) break;
			active = key.value;
		}
		break;
	}
	return active;
}

VMDLModel::VmdlColliderAnimationTrack& VMDLModel::GetOrCreateColliderAnimationTrack(
	const std::string& animationName, int colliderIndex)
{
	for (auto& track : vmdlAnimationControlData.colliderTracks)
	{
		if (track.animationName == animationName && track.colliderIndex == colliderIndex)
			return track;
	}
	auto& track = vmdlAnimationControlData.colliderTracks.emplace_back();
	track.animationName = animationName;
	track.colliderIndex = colliderIndex;
	return track;
}

bool VMDLModel::GetTrailInitialActive(int trailIndex) const
{
	if (trailIndex < 0) return true;
	if (trailIndex >= static_cast<int>(vmdlTrailData.initialActive.size())) return true;
	return vmdlTrailData.initialActive[trailIndex] != 0;
}

void VMDLModel::SetTrailInitialActive(int trailIndex, bool active)
{
	if (trailIndex < 0) return;
	auto& values = vmdlTrailData.initialActive;
	if (values.size() <= static_cast<size_t>(trailIndex)) values.resize(trailIndex + 1, 1);
	values[trailIndex] = active ? 1 : 0;
}

bool VMDLModel::EvaluateTrailActive(int animationIndex, float time, int trailIndex) const
{
	bool active = GetTrailInitialActive(trailIndex);
	if (animationIndex < 0 || animationIndex >= static_cast<int>(animations.size())) return active;

	const Animation& animation = animations[animationIndex];
	if (animation.secondsLength > 0.0f)
	{
		while (time < 0.0f) time += animation.secondsLength;
		while (time > animation.secondsLength) time -= animation.secondsLength;
	}
	for (const auto& track : vmdlTrailData.tracks)
	{
		if (track.animationName != animation.name || track.trailIndex != trailIndex) continue;
		for (const auto& key : track.keys)
		{
			if (key.seconds > time) break;
			active = key.value;
		}
		break;
	}
	return active;
}

VMDLModel::VmdlTrailAnimationTrack& VMDLModel::GetOrCreateTrailAnimationTrack(
	const std::string& animationName, int trailIndex)
{
	for (auto& track : vmdlTrailData.tracks)
	{
		if (track.animationName == animationName && track.trailIndex == trailIndex) return track;
	}
	auto& track = vmdlTrailData.tracks.emplace_back();
	track.animationName = animationName;
	track.trailIndex = trailIndex;
	return track;
}

VMDLModel::VmdlMorphAnimationTrack& VMDLModel::GetOrCreateMorphAnimationTrack(
	const std::string& animationName)
{
	for (auto& track : vmdlAnimationControlData.morphTracks)
	{
		if (track.animationName == animationName) return track;
	}
	auto& track = vmdlAnimationControlData.morphTracks.emplace_back();
	track.animationName = animationName;
	return track;
}

const VMDLModel::VmdlMorphAnimationTrack* VMDLModel::FindMorphAnimationTrack(
	const std::string& animationName) const
{
	for (const auto& track : vmdlAnimationControlData.morphTracks)
	{
		if (track.animationName == animationName) return &track;
	}
	return nullptr;
}

void VMDLModel::CaptureRuntimeMorphVisibility()
{
	runtimeMorphVisibility.clear();
	runtimeMorphVisibility.reserve(meshes.size());
	for (const Mesh& mesh : meshes) runtimeMorphVisibility.push_back(mesh.isDraw ? 1 : 0);
}

bool VMDLModel::ApplyMorphToMeshes(int morphIndex)
{
	const auto& morphs = vmdlExtensionData.morphs;
	if (morphIndex < 0 || morphIndex >= static_cast<int>(morphs.size())) return false;

	const auto& visibility = morphs[morphIndex].meshVisibility;
	const size_t count = std::min(meshes.size(), visibility.size());
	for (size_t i = 0; i < count; ++i)
	{
		if (visibility[i] == 1) meshes[i].isDraw = true;
		else if (visibility[i] == 0) meshes[i].isDraw = false;
	}
	return true;
}

void VMDLModel::RestoreMorphVisibility(const std::vector<uint8_t>& visibility)
{
	const size_t count = std::min(meshes.size(), visibility.size());
	for (size_t i = 0; i < count; ++i) meshes[i].isDraw = visibility[i] != 0;
	CaptureRuntimeMorphVisibility();
}

void VMDLModel::RestoreRuntimeMorphVisibility()
{
	const size_t count = std::min(meshes.size(), runtimeMorphVisibility.size());
	for (size_t i = 0; i < count; ++i) meshes[i].isDraw = runtimeMorphVisibility[i] != 0;
}

void VMDLModel::ApplyMorphAnimation(int animationIndex, float time)
{
	RestoreRuntimeMorphVisibility();
	if (animationIndex < 0 || animationIndex >= static_cast<int>(animations.size())) return;

	const Animation& animation = animations[animationIndex];
	if (animation.secondsLength > 0.0f)
	{
		while (time < 0.0f) time += animation.secondsLength;
		while (time > animation.secondsLength) time -= animation.secondsLength;
	}

	const auto* track = FindMorphAnimationTrack(animation.name);
	if (!track) return;
	for (const auto& key : track->keys)
	{
		if (key.seconds > time) break;
		ApplyMorphToMeshes(key.morphIndex);
	}
}

std::vector<VMDLModel::VmdlMaterialData> VMDLModel::CaptureVmdlMaterialData() const
{
	std::vector<VmdlMaterialData> result;
	result.reserve(materials.size());
	for (const Material& material : materials)
	{
		result.push_back({material.name, material.baseColor, material.emissiveColor,
			material.metalness, material.roughness, material.occlusion, material.occlusionStrength,
			material.shadowStrength, material.alphaCutoff, material.alphaMode,
			material.fresnelColor, material.fresnelPower, material.fresnelStrength,
			material.isFlatShading});
	}
	return result;
}

void VMDLModel::ApplyVmdlMaterialData(const std::vector<VmdlMaterialData>& data)
{
	std::unordered_map<std::string, const VmdlMaterialData*> lookup;
	lookup.reserve(data.size());
	for (const VmdlMaterialData& value : data) lookup.emplace(value.name, &value);

	for (Material& material : materials)
	{
		const auto found = lookup.find(material.name);
		if (found == lookup.end()) continue;
		const VmdlMaterialData& value = *found->second;
		material.baseColor = value.baseColor;
		material.emissiveColor = value.emissiveColor;
		material.metalness = value.metalness;
		material.roughness = value.roughness;
		material.occlusion = value.occlusion;
		material.occlusionStrength = value.occlusionStrength;
		material.shadowStrength = value.shadowStrength;
		material.alphaCutoff = value.alphaCutoff;
		material.alphaMode = value.alphaMode;
		material.fresnelColor = value.fresnelColor;
		material.fresnelPower = value.fresnelPower;
		material.fresnelStrength = value.fresnelStrength;
		material.isFlatShading = value.isFlatShading;
	}
}

bool VMDLModel::ReplaceGLBCache(const std::filesystem::path& filepath, float sampleRate)
{
	// 入力を確認
	std::string extension = ToUpperAscii(filepath.extension().string());
	if ((extension != ".GLB" && extension != ".GLTF") || !std::filesystem::exists(filepath))
		return false;

	// 新GLBを先に読込、VMDL側の編集値は後で同名マテリアルへ戻す
	const std::vector<VmdlMaterialData> materialData = CaptureVmdlMaterialData();
	VMDLModel replacement(filepath.string().c_str(), sampleRate);
	const std::vector<Node> oldNodes = nodes;
	// ノード名を索引化
	std::unordered_map<std::string, int> newNodeIndices;
	newNodeIndices.reserve(replacement.nodes.size());
	for (int i = 0; i < static_cast<int>(replacement.nodes.size()); ++i)
		newNodeIndices.emplace(replacement.nodes[i].name, i);
	auto remapNode = [&](int oldIndex) {
		if (oldIndex < 0 || oldIndex >= static_cast<int>(oldNodes.size())) return -1;
		const auto found = newNodeIndices.find(oldNodes[oldIndex].name);
		return found == newNodeIndices.end() ? -1 : found->second;
	};

	// 独自データを再接続
	for (auto& value : vmdlExtensionData.rigidBodies) value.nodeIndex = remapNode(value.nodeIndex);
	for (auto& value : vmdlExtensionData.colliders) value.nodeIndex = remapNode(value.nodeIndex);
	for (auto& value : vmdlExtensionData.springs) value.nodeIndex = remapNode(value.nodeIndex);
	for (auto& value : vmdlExtensionData.springColliders)
		value.nodeIndex = remapNode(value.nodeIndex);
	for (auto& value : vmdlTrailData.trails) value.nodeIndex = remapNode(value.nodeIndex);

	// GLB部分を交換
	sourceMaterials = replacement.sourceMaterials;
	if (sourceMaterials.empty()) sourceMaterials = replacement.materials;
	materials = std::move(replacement.materials);
	meshes = std::move(replacement.meshes);
	nodes = std::move(replacement.nodes);
	animations = std::move(replacement.animations);
	ApplyVmdlMaterialData(materialData);
	RebuildRuntimeReferences();
	return true;
}

void VMDLModel::ApplyForwardDirectionCorrection()
{
	const Matrix correction = Matrix::CreateRotationY(DirectX::XM_PI);
	for (int nodeIndex = 0; nodeIndex < static_cast<int>(nodes.size()); ++nodeIndex)
	{
		Node& node = nodes[nodeIndex];
		if (node.parentIndex >= 0) continue;

		const Matrix local = Matrix::CreateScale(node.scale) *
							 Matrix::CreateFromQuaternion(node.rotation) *
							 Matrix::CreateTranslation(node.position);
		(local * correction).Decompose(node.scale, node.rotation, node.position);
		node.rotation.Normalize();

		for (Animation& animation : animations)
		{
			if (nodeIndex >= static_cast<int>(animation.nodeAnims.size())) continue;
			NodeAnim& nodeAnimation = animation.nodeAnims[nodeIndex];
			for (VectorKeyframe& key : nodeAnimation.positionKeyframes)
				key.value = Vector3::Transform(key.value, correction);
			for (QuaternionKeyframe& key : nodeAnimation.rotationKeyframes)
			{
				key.value = Quaternion::CreateFromRotationMatrix(
					Matrix::CreateFromQuaternion(key.value) * correction);
				key.value.Normalize();
			}
		}
	}
}

bool VMDLModel::SaveVmdl()
{
	if (modelCacheFilepath.empty()) return false;

	Serialize(modelCacheFilepath.string().c_str());
	return true;
}

bool VMDLModel::SaveVmdl(const std::filesystem::path& filepath)
{
	if (filepath.empty()) return false;
	modelCacheFilepath = filepath;
	return SaveVmdl();
}

void VMDLModel::SetNodePoses(const std::vector<NodePose>& nodePoses)
{
	for (size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex)
	{
		const NodePose& pose = nodePoses.at(nodeIndex);
		Node& node = nodes.at(nodeIndex);

		node.position = pose.position;
		node.rotation = pose.rotation;
		node.scale = pose.scale;
	}
}

void VMDLModel::GetNodePoses(std::vector<NodePose>& nodePoses) const
{
	if (nodePoses.size() != nodes.size())
	{
		nodePoses.resize(nodes.size());
	}
	for (size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex)
	{
		const Node& node = nodes.at(nodeIndex);
		NodePose& pose = nodePoses.at(nodeIndex);

		pose.position = node.position;
		pose.rotation = node.rotation;
		pose.scale = node.scale;
	}
}

void VMDLModel::Serialize(const char* filename)
{
	NormalizeAttachmentNames();
	NormalizeMorphNames();
	std::ostringstream serializedStream(std::ios::binary | std::ios::out);
	const std::vector<VmdlMaterialData> materialData = CaptureVmdlMaterialData();
	const std::vector<Material>& glbMaterials =
		sourceMaterials.empty() ? materials : sourceMaterials;

	try
	{
		// 内部ファイルを作成
		std::vector<std::pair<std::string, std::string>> files;
		auto addFile = [&](const char* name, auto&& write) {
			std::ostringstream stream(std::ios::binary | std::ios::out);
			cereal::BinaryOutputArchive archive(stream);
			write(archive);
			files.emplace_back(name, stream.str());
		};
		addFile("model.glbcache",
			[&](auto& archive) { archive(nodes, glbMaterials, meshes, animations); });
		addFile("model.vmdldata", [&](auto& archive) {
			archive(materialData, vmdlExtensionData, vmdlIKSettings, vmdlIKPoles, modelScale,
				vmdlTrailData, vmdlAnimationEditorData, vmdlAnimationControlData,
				vmdlIKRaySettings);
		});

		cereal::BinaryOutputArchive package(serializedStream);
		package(files);
	}
	catch (...)
	{
		_ASSERT_EXPR_A(false, "VMDLModel serialize failed.");
		return;
	}

	const std::string serializedData = serializedStream.str();
	COMPRESSOR_HANDLE compressor = nullptr;
	if (!CreateCompressor(COMPRESS_ALGORITHM_XPRESS_HUFF, nullptr, &compressor))
	{
		_ASSERT_EXPR_A(false, "VMDLModel compressor creation failed.");
		return;
	}

	SIZE_T compressedSize = 0;
	Compress(compressor, serializedData.data(), serializedData.size(), nullptr, 0, &compressedSize);

	if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || compressedSize == 0)
	{
		CloseCompressor(compressor);
		_ASSERT_EXPR_A(false, "VMDLModel compressed size calculation failed.");
		return;
	}

	std::vector<uint8_t> compressedData(compressedSize);
	if (!Compress(compressor, serializedData.data(), serializedData.size(), compressedData.data(),
			compressedData.size(), &compressedSize))
	{
		CloseCompressor(compressor);
		_ASSERT_EXPR_A(false, "VMDLModel compression failed.");
		return;
	}
	CloseCompressor(compressor);
	compressedData.resize(compressedSize);

	std::ofstream ostream(std::filesystem::path(filename), std::ios::binary | std::ios::trunc);
	if (!ostream.is_open())
	{
		_ASSERT_EXPR_A(false, "VMDLModel file open failed.");
		return;
	}

	static constexpr std::array<char, 8> magic = {'V', 'M', 'D', 'L', 'C', 'M', 'P', '\0'};
	const uint32_t version = VmdlCompressionVersion;
	const uint64_t uncompressedSize = static_cast<uint64_t>(serializedData.size());
	const uint64_t storedCompressedSize = static_cast<uint64_t>(compressedData.size());

	ostream.write(magic.data(), magic.size());
	ostream.write(reinterpret_cast<const char*>(&version), sizeof(version));
	ostream.write(reinterpret_cast<const char*>(&uncompressedSize), sizeof(uncompressedSize));
	ostream.write(
		reinterpret_cast<const char*>(&storedCompressedSize), sizeof(storedCompressedSize));
	ostream.write(reinterpret_cast<const char*>(compressedData.data()), compressedData.size());

	if (!ostream.good())
	{
		_ASSERT_EXPR_A(false, "VMDLModel file write failed.");
	}
}

void VMDLModel::Deserialize(const char* filename)
{
	std::ifstream fileStream(std::filesystem::path(filename), std::ios::binary);
	if (!fileStream.is_open())
	{
		_ASSERT_EXPR_A(false, "VMDLModel File not found.");
		return;
	}

	try
	{
		static constexpr std::array<char, 8> magic = {'V', 'M', 'D', 'L', 'C', 'M', 'P', '\0'};
		std::array<char, magic.size()> fileMagic{};
		fileStream.read(fileMagic.data(), fileMagic.size());

		if (fileStream.gcount() != static_cast<std::streamsize>(fileMagic.size()) ||
			fileMagic != magic)
			throw std::runtime_error("Invalid compressed VMDL magic.");

		{
			uint32_t version = 0;
			uint64_t uncompressedSize = 0;
			uint64_t compressedSize = 0;
			fileStream.read(reinterpret_cast<char*>(&version), sizeof(version));
			fileStream.read(reinterpret_cast<char*>(&uncompressedSize), sizeof(uncompressedSize));
			fileStream.read(reinterpret_cast<char*>(&compressedSize), sizeof(compressedSize));

			if (!fileStream.good() || version != VmdlCompressionVersion || uncompressedSize == 0 ||
				compressedSize == 0 ||
				uncompressedSize > static_cast<uint64_t>((std::numeric_limits<SIZE_T>::max)()) ||
				compressedSize > static_cast<uint64_t>((std::numeric_limits<SIZE_T>::max)()))
			{
				throw std::runtime_error("Invalid compressed VMDL header.");
			}
			std::vector<uint8_t> compressedData(static_cast<size_t>(compressedSize));
			fileStream.read(reinterpret_cast<char*>(compressedData.data()),
				static_cast<std::streamsize>(compressedData.size()));
			if (!fileStream.good())
			{
				throw std::runtime_error("Compressed VMDL data is truncated.");
			}

			std::vector<uint8_t> serializedData(static_cast<size_t>(uncompressedSize));
			DECOMPRESSOR_HANDLE decompressor = nullptr;
			if (!CreateDecompressor(COMPRESS_ALGORITHM_XPRESS_HUFF, nullptr, &decompressor))
			{
				throw std::runtime_error("VMDL decompressor creation failed.");
			}

			SIZE_T decompressedSize = 0;
			const BOOL result =
				Decompress(decompressor, compressedData.data(), compressedData.size(),
					serializedData.data(), serializedData.size(), &decompressedSize);
			CloseDecompressor(decompressor);

			if (!result || decompressedSize != serializedData.size())
			{
				throw std::runtime_error("VMDL decompression failed.");
			}

			std::string serializedString(
				reinterpret_cast<const char*>(serializedData.data()), serializedData.size());
			std::istringstream serializedStream(serializedString, std::ios::binary | std::ios::in);
			std::vector<VmdlMaterialData> materialData;
			std::vector<std::pair<std::string, std::string>> files;
			cereal::BinaryInputArchive package(serializedStream);
			package(files);
			bool loadedGlbCache = false;
			bool loadedVmdlData = false;
			for (const auto& [name, data] : files)
			{
				std::istringstream section(data, std::ios::binary | std::ios::in);
				cereal::BinaryInputArchive archive(section);
				if (name == "model.glbcache")
				{
					archive(nodes, materials, meshes, animations);
					loadedGlbCache = true;
				}
				else if (name == "model.vmdldata")
				{
					archive(materialData, vmdlExtensionData, vmdlIKSettings, vmdlIKPoles,
						modelScale, vmdlTrailData, vmdlAnimationEditorData,
						vmdlAnimationControlData, vmdlIKRaySettings);
					loadedVmdlData = true;
				}
			}
			if (!loadedGlbCache || !loadedVmdlData)
				throw std::runtime_error("VMDL package is missing required data.");
			sourceMaterials = materials;
			ApplyVmdlMaterialData(materialData);
			SetModelScale(modelScale);
			NormalizeAttachmentNames();
			NormalizeVmdlIKRaySettings();
			NormalizeMorphNames();
		}
	}
	catch (...)
	{
		_ASSERT_EXPR_A(false, "VMDLModel deserialize failed.");
	}
}
