// VMDLModel.cpp

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
#include <limits>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace
{
	std::string ToUpperAscii(std::string value)
	{
		std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
		{
			return static_cast<char>(std::toupper(c));
		});
		return value;
	}
}

uint64_t VMDLModel::MakeModelCacheStamp(uint64_t sourceLastWrite)
{
	return sourceLastWrite ^ ModelCacheVersion;
}

bool VMDLModel::ReadModelCacheStamp(const std::filesystem::path& filepath, uint64_t& stamp)
{
	std::ifstream stream(filepath, std::ios::binary);
	if (!stream.is_open()) return false;

	stream.read(reinterpret_cast<char*>(&stamp), sizeof(stamp));
	return stream.good();
}

bool VMDLModel::IsCacheUpToDate(
	const std::filesystem::path& sourcePath,
	const std::filesystem::path& cachePath)
{
	if (!std::filesystem::exists(sourcePath) || !std::filesystem::exists(cachePath)) return false;

	uint64_t cachedStamp = 0;
	return ReadModelCacheStamp(cachePath, cachedStamp) &&
		cachedStamp == MakeModelCacheStamp(GetFileLastWriteTime64(sourcePath));
}

void VMDLModel::BuildEmbeddedDDSFromFileOrSRV(
	ID3D11Device* device,
	const std::filesystem::path& dirpath,
	const std::string& textureFileName,
	ID3D11ShaderResourceView* srv,
	std::vector<uint8_t>& outDDS)
{
	outDDS.clear();

	// 再現性のある元ファイルを優先する。GLB内蔵画像など実ファイルがない場合だけ、
	// importerが作成したGPUテクスチャを読み戻してDDS化する。
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
	ID3D11Device* device,
	const std::filesystem::path& dirpath,
	VMDLModel::Material& material)
{
	BuildEmbeddedDDSFromFileOrSRV(
		device,
		dirpath,
		material.baseTextureFileName,
		material.baseMap.Get(),
		material.baseTextureDDS
	);

	BuildEmbeddedDDSFromFileOrSRV(
		device,
		dirpath,
		material.normalTextureFileName,
		material.normalMap.Get(),
		material.normalTextureDDS
	);

	BuildEmbeddedDDSFromFileOrSRV(
		device,
		dirpath,
		material.emissiveTextureFileName,
		material.emissiveMap.Get(),
		material.emissiveTextureDDS
	);

	BuildEmbeddedDDSFromFileOrSRV(
		device,
		dirpath,
		material.occlusionTextureFileName,
		material.occlusionMap.Get(),
		material.occlusionTextureDDS
	);

	BuildEmbeddedDDSFromFileOrSRV(
		device,
		dirpath,
		material.metalnessRoughnessTextureFileName,
		material.metalnessRoughnessMap.Get(),
		material.metalnessRoughnessTextureDDS
	);
}

void VMDLModel::CreateSRVFromEmbeddedDDSOrFile(
	ID3D11Device* device,
	const std::filesystem::path& dirpath,
	const std::string& textureFileName,
	const std::vector<uint8_t>& embeddedDDS,
	uint32_t dummyColor,
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
			device,
			embeddedDDS.data(),
			embeddedDDS.size(),
			nullptr,
			srv.GetAddressOf()
		);

		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
		return;
	}

	if (textureFileName.empty())
	{
		hr = GpuResourceUtils::CreateDummyTexture(
			device,
			dummyColor,
			srv.GetAddressOf()
		);

		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
		return;
	}

	std::filesystem::path texturePath = dirpath / textureFileName;

	hr = GpuResourceUtils::LoadTexture(
		device,
		texturePath.string().c_str(),
		srv.GetAddressOf()
	);

	_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
}

void VMDLModel::BuildMaterialTextureResources(
	ID3D11Device* device,
	const std::filesystem::path& dirpath,
	VMDLModel::Material& material)
{
	CreateSRVFromEmbeddedDDSOrFile(
		device,
		dirpath,
		material.baseTextureFileName,
		material.baseTextureDDS,
		0xFFFFFFFF,
		material.baseMap
	);

	CreateSRVFromEmbeddedDDSOrFile(
		device,
		dirpath,
		material.normalTextureFileName,
		material.normalTextureDDS,
		0xFFFF7F7F,
		material.normalMap
	);

	CreateSRVFromEmbeddedDDSOrFile(
		device,
		dirpath,
		material.emissiveTextureFileName,
		material.emissiveTextureDDS,
		0xFF000000,
		material.emissiveMap
	);

	CreateSRVFromEmbeddedDDSOrFile(
		device,
		dirpath,
		material.occlusionTextureFileName,
		material.occlusionTextureDDS,
		0xFFFFFFFF,
		material.occlusionMap
	);

	CreateSRVFromEmbeddedDDSOrFile(
		device,
		dirpath,
		material.metalnessRoughnessTextureFileName,
		material.metalnessRoughnessTextureDDS,
		0xFF00FF00,
		material.metalnessRoughnessMap
	);
}

bool VMDLModel::ReplaceMaterialTexture(
	size_t materialIndex,
	MaterialTextureSlot slot,
	const std::filesystem::path& texturePath)
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
	if (FAILED(ConvertTextureFileToDDSBytes(texturePath, convertedDDS)) || convertedDDS.empty()) return false;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> replacement;
	const HRESULT hr = DirectX::CreateDDSTextureFromMemory(
		Game::Graphics::Instance().GetDevice(),
		convertedDDS.data(),
		convertedDDS.size(),
		nullptr,
		replacement.GetAddressOf());
	if (FAILED(hr)) return false;

	*filename = texturePath.filename().string();
	*embeddedDDS = std::move(convertedDDS);
	*srv = std::move(replacement);
	return true;
}

bool VMDLModel::ExportMaterialTexture(
	size_t materialIndex,
	MaterialTextureSlot slot,
	const std::filesystem::path& savePath)
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
		std::ofstream file(
			savePath,
			std::ios::binary | std::ios::out | std::ios::trunc);

		if (!file)
		{
			return false;
		}

		file.write(
			reinterpret_cast<const char*>(embeddedDDS->data()),
			static_cast<std::streamsize>(embeddedDDS->size()));
	}
	else if(ext == ".png")
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
		Game::Graphics::Instance().GetDevice(),
		dummyColor,
		replacement.GetAddressOf()))) return false;

	filename->clear();
	embeddedDDS->clear();
	*srv = std::move(replacement);
	return true;
}

VMDLModel::VMDLModel(
	const char* filename,
	float sampleRate,
	bool importRawModel,
	const char* cacheFilename,
	bool saveImportedCache)
{
	auto device = Game::Graphics::Instance().GetDevice();

	std::filesystem::path sourceFilepath(filename);
	std::filesystem::path dirpath(sourceFilepath.parent_path());
	std::filesystem::path extension = sourceFilepath.extension();

	std::filesystem::path cerealFilepath;
	if (cacheFilename && cacheFilename[0] != '\0')
	{
		cerealFilepath = cacheFilename;
	}
	else
	{
		cerealFilepath = sourceFilepath;
		cerealFilepath.replace_extension(".vmdl");
	}
	modelCacheFilepath = cerealFilepath;
	modelCacheLastWrite = std::filesystem::exists(filename)
		? MakeModelCacheStamp(GetFileLastWriteTime64(filename))
		: 0;

	if (extension == ".vmdl" && std::filesystem::exists(sourceFilepath))
	{
		uint64_t lastWriteTime = 0;
		Deserialize(sourceFilepath.string().c_str(), lastWriteTime);
		modelCacheLastWrite = lastWriteTime;
	}
	else if (std::filesystem::exists(cerealFilepath) && !importRawModel)
	{
		// 旧GLB運用との互換処理。元ファイルがある場合だけ更新時刻を照合し、
		// 不一致なら再インポートする。正式なVMDL読み込みではこの経路を通らない。
		bool canUseCache = true;
		if (std::filesystem::exists(filename))
		{
			uint64_t cachedLastWriteTime = 0;
			canUseCache =
				ReadModelCacheStamp(cerealFilepath, cachedLastWriteTime) &&
				cachedLastWriteTime == modelCacheLastWrite;
		}

		if (!canUseCache)
		{
			VMDLModel tmpModel(filename, sampleRate, true, cerealFilepath.string().c_str());
			*this = std::move(tmpModel);
			return;
		}

		uint64_t lastWriteTime = 0;
		Deserialize(cerealFilepath.string().c_str(), lastWriteTime);
		modelCacheLastWrite = lastWriteTime;
	}
	else if (extension == ".gltf" || extension == ".glb")
	{
		// GLB内蔵画像を含む全テクスチャをDDSへ変換し、VMDL単体で描画できるよう埋め込む。
		GLTFImporter importer(filename);

		importer.LoadMaterials(materials, device);

		importer.LoadNodes(nodes);

		importer.LoadMeshes(meshes, nodes);

		importer.LoadAnimations(animations, nodes, sampleRate);

		for (Material& material : materials)
		{
			BuildMaterialEmbeddedDDS(device, dirpath, material);
		}

		if (saveImportedCache)
		{
			Serialize(
				cerealFilepath.string().c_str(),
				MakeModelCacheStamp(GetFileLastWriteTime64(filename))
			);
		}
	}
	else
	{
		_ASSERT_EXPR_A(false, "found not model file");
	}

	// シリアライズ対象はインデックスだけなので、全配列が揃ってからポインタ参照を再構築する。
	// vector再配置による参照切れを避けるため、読み込み途中ではポインタを設定しない。
	for (size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex)
	{
		const int parentIndex = nodes[nodeIndex].parentIndex;
		if (parentIndex >= static_cast<int>(nodes.size()))
			throw std::runtime_error("Invalid parent node index: node=" + std::to_string(nodeIndex) + ", parent=" + std::to_string(parentIndex));
	}
	for (size_t meshIndex = 0; meshIndex < meshes.size(); ++meshIndex)
	{
		const Mesh& mesh = meshes[meshIndex];
		if (mesh.materialIndex < 0 || mesh.materialIndex >= static_cast<int>(materials.size()))
			throw std::runtime_error("Invalid material index: mesh=" + std::to_string(meshIndex) + ", material=" + std::to_string(mesh.materialIndex) + ", count=" + std::to_string(materials.size()));
		if (mesh.nodeIndex < 0 || mesh.nodeIndex >= static_cast<int>(nodes.size()))
			throw std::runtime_error("Invalid mesh node index: mesh=" + std::to_string(meshIndex) + ", node=" + std::to_string(mesh.nodeIndex) + ", count=" + std::to_string(nodes.size()));
		for (size_t boneIndex = 0; boneIndex < mesh.bones.size(); ++boneIndex)
		{
			const int nodeIndex = mesh.bones[boneIndex].nodeIndex;
			if (nodeIndex < 0 || nodeIndex >= static_cast<int>(nodes.size()))
				throw std::runtime_error("Invalid bone node index: mesh=" + std::to_string(meshIndex) + ", bone=" + std::to_string(boneIndex) + ", node=" + std::to_string(nodeIndex) + ", count=" + std::to_string(nodes.size()));
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
				&bufferDesc,
				&subresourceData,
				mesh.vertexBuffer.GetAddressOf()
			);

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
				&bufferDesc,
				&subresourceData,
				mesh.indexBuffer.GetAddressOf()
			);

			_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
		}

		for (Bone& bone : mesh.bones)
		{
			bone.node = &nodes.at(bone.nodeIndex);
		}
	}

	UpdateTransform(Matrix::Identity);
	CaptureRuntimeMorphVisibility();
}

VMDLModel::VMDLModel(const VMDLModel& other)
	: materials(other.materials),
	meshes(other.meshes),
	nodes(other.nodes),
	animations(other.animations),
	vmdlExtensionData(other.vmdlExtensionData),
	vmdlIKSettings(other.vmdlIKSettings),
	vmdlAnimationEditorData(other.vmdlAnimationEditorData),
	vmdlAnimationControlData(other.vmdlAnimationControlData),
	vmdlTrailData(other.vmdlTrailData),
	modelScale(other.modelScale),
	worldTransform(other.worldTransform),
	modelCacheFilepath(other.modelCacheFilepath),
	modelCacheLastWrite(other.modelCacheLastWrite)
{
	RebuildRuntimeReferences();
}

VMDLModel::VMDLModel(VMDLModel&& other) noexcept
	: materials(std::move(other.materials)),
	meshes(std::move(other.meshes)),
	nodes(std::move(other.nodes)),
	animations(std::move(other.animations)),
	vmdlExtensionData(std::move(other.vmdlExtensionData)),
	vmdlIKSettings(std::move(other.vmdlIKSettings)),
	vmdlAnimationEditorData(std::move(other.vmdlAnimationEditorData)),
	vmdlAnimationControlData(std::move(other.vmdlAnimationControlData)),
	vmdlTrailData(std::move(other.vmdlTrailData)),
	modelScale(other.modelScale),
	worldTransform(other.worldTransform),
	modelCacheFilepath(std::move(other.modelCacheFilepath)),
	modelCacheLastWrite(other.modelCacheLastWrite)
{
	RebuildRuntimeReferences();
}

VMDLModel& VMDLModel::operator=(const VMDLModel& other)
{
	if (this == &other) return *this;

	materials = other.materials;
	meshes = other.meshes;
	nodes = other.nodes;
	animations = other.animations;
	vmdlExtensionData = other.vmdlExtensionData;
	vmdlIKSettings = other.vmdlIKSettings;
	vmdlAnimationEditorData = other.vmdlAnimationEditorData;
	vmdlAnimationControlData = other.vmdlAnimationControlData;
	vmdlTrailData = other.vmdlTrailData;
	modelScale = other.modelScale;
	worldTransform = other.worldTransform;
	modelCacheFilepath = other.modelCacheFilepath;
	modelCacheLastWrite = other.modelCacheLastWrite;
	RebuildRuntimeReferences();
	return *this;
}

VMDLModel& VMDLModel::operator=(VMDLModel&& other) noexcept
{
	if (this == &other) return *this;

	materials = std::move(other.materials);
	meshes = std::move(other.meshes);
	nodes = std::move(other.nodes);
	animations = std::move(other.animations);
	vmdlExtensionData = std::move(other.vmdlExtensionData);
	vmdlIKSettings = std::move(other.vmdlIKSettings);
	vmdlAnimationEditorData = std::move(other.vmdlAnimationEditorData);
	vmdlAnimationControlData = std::move(other.vmdlAnimationControlData);
	vmdlTrailData = std::move(other.vmdlTrailData);
	modelScale = other.modelScale;
	worldTransform = other.worldTransform;
	modelCacheFilepath = std::move(other.modelCacheFilepath);
	modelCacheLastWrite = other.modelCacheLastWrite;
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

	if (filepath.extension() == ".gltf" ||
		filepath.extension() == ".glb")
	{
		GLTFImporter importer(filename);

		std::vector<Node> animNodes;
		importer.LoadNodes(animNodes);

		std::vector<Animation> newAnims;
		importer.LoadAnimations(newAnims, animNodes);

		std::unordered_map<std::string, int> modelNodeMap;
		for (int i = 0; i < (int)nodes.size(); ++i)
			modelNodeMap[nodes[i].name] = i;

		for (Animation& anim : newAnims)
		{
			// アニメーション側とモデル側ではノード順が一致する保証がない。
			// 全ノードを初期姿勢で埋め、同名ノードだけ読み込んだキーへ差し替える。
			// これによりアニメーションにない装備ボーンも初期姿勢を維持できる。
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
	char buffer[256];
	sprintf_s(buffer, "animation not found: %s", name);
	_ASSERT(buffer);
	return -1;
}

static void UpdateNodeTransform(VMDLModel::Node& node, const Matrix& parentGlobal, const Matrix& worldTransform)
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
		if (result.ec == std::errc() && result.ptr == separator &&
			referencedIndex >= 0 && referencedIndex < static_cast<int>(nodes.size()) &&
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

bool VMDLModel::ApplyMorph(const char* name)
{
	return ApplyMorph(GetMorphIndex(name));
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

	return
		Matrix::CreateTranslation(-pivot) *
		Matrix::CreateScale(modelScale) *
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

void VMDLModel::ComputeAnimation(int animationIndex, int nodeIndex, float time, NodePose& nodePose) const
{
	const Animation& animation = animations.at(animationIndex);
	const NodeAnim& nodeAnim = animation.nodeAnims.at(nodeIndex);

	for (size_t index = 0; index < nodeAnim.positionKeyframes.size() - 1; ++index)
	{
		const VectorKeyframe& keyframe0 = nodeAnim.positionKeyframes.at(index);
		const VectorKeyframe& keyframe1 = nodeAnim.positionKeyframes.at(index + 1);
		if (time >= keyframe0.seconds && time <= keyframe1.seconds)
		{
			float rate = (time - keyframe0.seconds) / (keyframe1.seconds - keyframe0.seconds);

			nodePose.position = Vector3::Lerp(keyframe0.value, keyframe1.value, rate);
		}
	}
	for (size_t index = 0; index < nodeAnim.rotationKeyframes.size() - 1; ++index)
	{
		const QuaternionKeyframe& keyframe0 = nodeAnim.rotationKeyframes.at(index);
		const QuaternionKeyframe& keyframe1 = nodeAnim.rotationKeyframes.at(index + 1);
		if (time >= keyframe0.seconds && time <= keyframe1.seconds)
		{
			float rate = (time - keyframe0.seconds) / (keyframe1.seconds - keyframe0.seconds);

			nodePose.rotation = Quaternion::Slerp(keyframe0.value, keyframe1.value, rate);
		}
	}
	for (size_t index = 0; index < nodeAnim.scaleKeyframes.size() - 1; ++index)
	{
		const VectorKeyframe& keyframe0 = nodeAnim.scaleKeyframes.at(index);
		const VectorKeyframe& keyframe1 = nodeAnim.scaleKeyframes.at(index + 1);
		if (time >= keyframe0.seconds && time <= keyframe1.seconds)
		{
			float rate = (time - keyframe0.seconds) / (keyframe1.seconds - keyframe0.seconds);

			nodePose.scale = Vector3::Lerp(keyframe0.value, keyframe1.value, rate);
		}
	}
}

void VMDLModel::ComputeAnimation(int animationIndex, float time, std::vector<NodePose>& nodePoses) const
{
	if (nodePoses.size() != nodes.size())
	{
		nodePoses.resize(nodes.size());
	}
	for (size_t nodeIndex = 0; nodeIndex < nodePoses.size(); ++nodeIndex)
	{
		ComputeAnimation(animationIndex, static_cast<int>(nodeIndex), time, nodePoses.at(nodeIndex));
	}
}

void VMDLModel::ResetVmdlIKLegsForType()
{
	auto& settings = vmdlIKSettings;
	settings.legs.clear();

	int legCount = 0;
	if (settings.type == 1) legCount = 2;
	else if (settings.type == 2) legCount = 4;
	else if (settings.type == 3) legCount = 8;
	settings.legs.resize(legCount);

	constexpr const char* humanNames[] = {"Left", "Right"};
	constexpr const char* quadrupedNames[] = {"Front Left", "Front Right", "Back Left", "Back Right"};
	for (int i = 0; i < legCount; ++i)
	{
		if (settings.type == 1) settings.legs[i].name = humanNames[i];
		else if (settings.type == 2) settings.legs[i].name = quadrupedNames[i];
		else settings.legs[i].name = "Leg " + std::to_string(i + 1);
	}

}

void VMDLModel::EnsureVmdlIKSettingsCompatibility()
{
	auto& settings = vmdlIKSettings;
	if (settings.centerNode.empty()) settings.centerNode = "pelvis";
	size_t requiredCount = 0;
	if (settings.type == 1) requiredCount = 2;
	else if (settings.type == 2) requiredCount = 4;
	else if (settings.type == 3) requiredCount = 8;
	settings.legs.resize(requiredCount);

	constexpr const char* humanNames[] = {"Left", "Right"};
	constexpr const char* quadrupedNames[] = {"Front Left", "Front Right", "Back Left", "Back Right"};
	for (size_t i = 0; i < settings.legs.size(); ++i)
	{
		if (!settings.legs[i].name.empty() && settings.legs[i].name != "Leg") continue;
		if (settings.type == 1) settings.legs[i].name = humanNames[i];
		else if (settings.type == 2) settings.legs[i].name = quadrupedNames[i];
		else settings.legs[i].name = "Leg " + std::to_string(i + 1);
	}

	// 旧形式では同名ノードを名前だけで保存していたため、複数の脚がすべて
	// 最初の同名ノードへ解決される。複数脚で同じ旧形式名を使っている場合だけ、
	// ノードの出現順に番号付き参照へ移行する。
	const auto upgradeDuplicateLegNodes = [this, &settings](std::string VmdlIKLeg::* member)
	{
		std::unordered_map<std::string, std::vector<std::string*>> referencesByName;
		for (VmdlIKLeg& leg : settings.legs)
		{
			std::string& reference = leg.*member;
			if (reference.empty() || reference.find(':') != std::string::npos) continue;
			referencesByName[reference].push_back(&reference);
		}

		for (auto& [nodeName, references] : referencesByName)
		{
			if (references.size() < 2) continue;

			std::vector<int> matchingNodeIndices;
			for (int nodeIndex = 0; nodeIndex < static_cast<int>(nodes.size()); ++nodeIndex)
			{
				if (nodes[nodeIndex].name == nodeName) matchingNodeIndices.push_back(nodeIndex);
			}
			if (matchingNodeIndices.size() < references.size()) continue;

			for (size_t referenceIndex = 0; referenceIndex < references.size(); ++referenceIndex)
			{
				const int nodeIndex = matchingNodeIndices[referenceIndex];
				*references[referenceIndex] = std::to_string(nodeIndex) + ":" + nodeName;
			}
		}
	};

	upgradeDuplicateLegNodes(&VmdlIKLeg::root);
	upgradeDuplicateLegNodes(&VmdlIKLeg::mid);
	upgradeDuplicateLegNodes(&VmdlIKLeg::tip);
	upgradeDuplicateLegNodes(&VmdlIKLeg::contact);
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
	if (!selectedTrack)
	{
		// 旧VMDLの単一トラックは全ての足へ共通適用する。
		for (const VmdlFootWeightTrack& track : vmdlAnimationEditorData.footWeightTracks)
		{
			if (track.animationName == animation.name)
			{
				selectedTrack = &track;
				break;
			}
		}
	}
	if (selectedTrack && !selectedTrack->weights.empty())
	{
		// ペイント済みトラックがあれば隣接サンプルを線形補間する。
		// 正負の値はそのまま返し、左右足などの解釈は利用側へ任せる。
		const auto& track = *selectedTrack;
		const float sample = std::clamp(time * track.sampleRate, 0.0f, static_cast<float>(track.weights.size() - 1));
		const size_t index0 = static_cast<size_t>(sample);
		const size_t index1 = std::min(index0 + 1, track.weights.size() - 1);
		return std::lerp(track.weights[index0], track.weights[index1], sample - static_cast<float>(index0));
	}

	// ペイントデータがない旧形式では区間設定へフォールバックし、
	// 重複区間のうち最も強いウェイトを採用する。
	float result = 0.0f;
	for (const FootIKRange& range : animation.footIKRanges)
	{
		if (range.footIndex >= 0 && range.footIndex != footIndex) continue;

		float timeRatio =
			animation.secondsLength > 0.001f
			? time / animation.secondsLength
			: 0.0f;

		timeRatio = std::clamp(timeRatio, 0.0f, 1.0f);

		float start = range.startRatio;
		float end = range.endRatio;
		if (end < start)
		{
			const float temp = start;
			start = end;
			end = temp;
		}

		if (timeRatio < start || timeRatio > end) continue;

		float weight = std::clamp(range.weight, 0.0f, 1.0f);

		if (range.fadeInRatio > 0.001f)
		{
			weight *= std::clamp((timeRatio - start) / range.fadeInRatio, 0.0f, 1.0f);
		}

		if (range.fadeOutRatio > 0.001f)
		{
			weight *= std::clamp((end - timeRatio) / range.fadeOutRatio, 0.0f, 1.0f);
		}

		if (result < weight) result = weight;
	}

	return result;
}

std::string VMDLModel::MakeFootWeightTrackKey(const std::string& animationName, int footIndex)
{
	return animationName + "::FootWeight:" + std::to_string(footIndex);
}

VMDLModel::VmdlFootWeightTrack* VMDLModel::FindFootWeightTrack(const std::string& animationName, int footIndex)
{
	const std::string key = MakeFootWeightTrackKey(animationName, footIndex);
	for (auto& track : vmdlAnimationEditorData.footWeightTracks)
	{
		if (track.animationName == key) return &track;
	}
	return nullptr;
}

const VMDLModel::VmdlFootWeightTrack* VMDLModel::FindFootWeightTrack(const std::string& animationName, int footIndex) const
{
	const std::string key = MakeFootWeightTrackKey(animationName, footIndex);
	for (const auto& track : vmdlAnimationEditorData.footWeightTracks)
	{
		if (track.animationName == key) return &track;
	}
	return nullptr;
}

VMDLModel::VmdlFootWeightTrack& VMDLModel::GetOrCreateFootWeightTrack(const std::string& animationName, int footIndex)
{
	if (auto* track = FindFootWeightTrack(animationName, footIndex)) return *track;
	auto& track = vmdlAnimationEditorData.footWeightTracks.emplace_back();
	track.animationName = MakeFootWeightTrackKey(animationName, footIndex);
	return track;
}

bool VMDLModel::GetColliderInitialActive(int colliderIndex) const
{
	if (colliderIndex < 0) return true;
	if (colliderIndex >= static_cast<int>(vmdlAnimationControlData.colliderInitialActive.size())) return true;
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

VMDLModel::VmdlColliderAnimationTrack& VMDLModel::GetOrCreateColliderAnimationTrack(const std::string& animationName, int colliderIndex)
{
	for (auto& track : vmdlAnimationControlData.colliderTracks)
	{
		if (track.animationName == animationName && track.colliderIndex == colliderIndex) return track;
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

VMDLModel::VmdlTrailAnimationTrack& VMDLModel::GetOrCreateTrailAnimationTrack(const std::string& animationName, int trailIndex)
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

VMDLModel::VmdlMorphAnimationTrack& VMDLModel::GetOrCreateMorphAnimationTrack(const std::string& animationName)
{
	for (auto& track : vmdlAnimationControlData.morphTracks)
	{
		if (track.animationName == animationName) return track;
	}
	auto& track = vmdlAnimationControlData.morphTracks.emplace_back();
	track.animationName = animationName;
	return track;
}

const VMDLModel::VmdlMorphAnimationTrack* VMDLModel::FindMorphAnimationTrack(const std::string& animationName) const
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
	// Morphは差分指定なので、現在時刻までのキーを先頭から順に適用して結果を再現する。
	for (const auto& key : track->keys)
	{
		if (key.seconds > time) break;
		ApplyMorphToMeshes(key.morphIndex);
	}
}

bool VMDLModel::SaveVmdl()
{
	if (modelCacheFilepath.empty()) return false;

	Serialize(
		modelCacheFilepath.string().c_str(),
		modelCacheLastWrite);
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

void VMDLModel::Serialize(const char* filename, uint64_t lastWrite)
{
	EnsureVmdlIKSettingsCompatibility();
	NormalizeMorphNames();
	std::ostringstream serializedStream(std::ios::binary | std::ios::out);
	std::vector<MaterialPbrSettings> materialPbrSettings;
	materialPbrSettings.reserve(materials.size());
	for (const Material& material : materials)
		materialPbrSettings.push_back({material.occlusion, material.shadowStrength});

	std::vector<MaterialVMatSettings> materialVMatSettings;
	materialVMatSettings.reserve(materials.size());
	for (const Material& material : materials)
	{
		materialVMatSettings.push_back({
			material.fresnelColor,
			material.fresnelPower,
			material.fresnelStrength,
			material.isFlatShading});
	}

	try
	{
		cereal::BinaryOutputArchive archive(serializedStream);
		archive(
			CEREAL_NVP(lastWrite),
			CEREAL_NVP(nodes),
			CEREAL_NVP(materials),
			CEREAL_NVP(meshes),
			CEREAL_NVP(animations),
			CEREAL_NVP(vmdlExtensionData),
			CEREAL_NVP(vmdlIKSettings),
			CEREAL_NVP(vmdlAnimationEditorData),
			CEREAL_NVP(vmdlAnimationControlData),
			CEREAL_NVP(materialPbrSettings),
			CEREAL_NVP(vmdlTrailData),
			CEREAL_NVP(materialVMatSettings),
			CEREAL_NVP(modelScale));
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
	Compress(
		compressor,
		serializedData.data(),
		serializedData.size(),
		nullptr,
		0,
		&compressedSize);

	if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || compressedSize == 0)
	{
		CloseCompressor(compressor);
		_ASSERT_EXPR_A(false, "VMDLModel compressed size calculation failed.");
		return;
	}

	std::vector<uint8_t> compressedData(compressedSize);
	if (!Compress(
		compressor,
		serializedData.data(),
		serializedData.size(),
		compressedData.data(),
		compressedData.size(),
		&compressedSize))
	{
		CloseCompressor(compressor);
		_ASSERT_EXPR_A(false, "VMDLModel compression failed.");
		return;
	}
	CloseCompressor(compressor);
	compressedData.resize(compressedSize);

	std::ofstream ostream(filename, std::ios::binary | std::ios::trunc);
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
	ostream.write(reinterpret_cast<const char*>(&storedCompressedSize), sizeof(storedCompressedSize));
	ostream.write(reinterpret_cast<const char*>(compressedData.data()), compressedData.size());

	if (!ostream.good())
	{
		_ASSERT_EXPR_A(false, "VMDLModel file write failed.");
	}
}

void VMDLModel::Deserialize(const char* filename, uint64_t& lastWrite)
{
	std::ifstream fileStream(filename, std::ios::binary);
	if (!fileStream.is_open())
	{
		_ASSERT_EXPR_A(false, "VMDLModel File not found.");
		return;
	}

	auto deserialize = [this, &lastWrite](std::istream& stream)
	{
		cereal::BinaryInputArchive archive(stream);

		archive(
			CEREAL_NVP(lastWrite),
			CEREAL_NVP(nodes),
			CEREAL_NVP(materials),
			CEREAL_NVP(meshes),
			CEREAL_NVP(animations));

		try
		{
			archive(CEREAL_NVP(vmdlExtensionData));
		}
		catch (...)
		{
			vmdlExtensionData = {};
		}

		try
		{
			archive(CEREAL_NVP(vmdlIKSettings));
		}
		catch (...)
		{
			vmdlIKSettings = {};
		}

		try
		{
			archive(CEREAL_NVP(vmdlAnimationEditorData));
		}
		catch (...)
		{
			vmdlAnimationEditorData = {};
		}

		try
		{
			archive(CEREAL_NVP(vmdlAnimationControlData));
		}
		catch (...)
		{
			vmdlAnimationControlData = {};
		}

		try
		{
			std::vector<MaterialPbrSettings> materialPbrSettings;
			archive(CEREAL_NVP(materialPbrSettings));
			const size_t count = std::min(materials.size(), materialPbrSettings.size());
			for (size_t i = 0; i < count; ++i)
			{
				materials[i].occlusion = materialPbrSettings[i].occlusion;
				materials[i].shadowStrength = materialPbrSettings[i].shadowStrength;
			}
		}
		catch (...)
		{
		}

		try
		{
			archive(CEREAL_NVP(vmdlTrailData));
		}
		catch (...)
		{
			vmdlTrailData = {};
		}

		try
		{
			std::vector<MaterialVMatSettings> materialVMatSettings;
			archive(CEREAL_NVP(materialVMatSettings));
			const size_t count = std::min(materials.size(), materialVMatSettings.size());
			for (size_t i = 0; i < count; ++i)
			{
				materials[i].fresnelColor = materialVMatSettings[i].fresnelColor;
				materials[i].fresnelPower = materialVMatSettings[i].fresnelPower;
				materials[i].fresnelStrength = materialVMatSettings[i].fresnelStrength;
				materials[i].isFlatShading = materialVMatSettings[i].isFlatShading;
			}
		}
		catch (...)
		{
		}

		try
		{
			archive(CEREAL_NVP(modelScale));
			SetModelScale(modelScale);
		}
		catch (...)
		{
			modelScale = 1.0f;
		}

		EnsureVmdlIKSettingsCompatibility();
		NormalizeMorphNames();
	};

	try
	{
		static constexpr std::array<char, 8> magic = {'V', 'M', 'D', 'L', 'C', 'M', 'P', '\0'};
		std::array<char, magic.size()> fileMagic{};
		fileStream.read(fileMagic.data(), fileMagic.size());

		if (fileStream.gcount() == static_cast<std::streamsize>(fileMagic.size()) && fileMagic == magic)
		{
			uint32_t version = 0;
			uint64_t uncompressedSize = 0;
			uint64_t compressedSize = 0;
			fileStream.read(reinterpret_cast<char*>(&version), sizeof(version));
			fileStream.read(reinterpret_cast<char*>(&uncompressedSize), sizeof(uncompressedSize));
			fileStream.read(reinterpret_cast<char*>(&compressedSize), sizeof(compressedSize));

			if (!fileStream.good() || version != VmdlCompressionVersion ||
				uncompressedSize == 0 || compressedSize == 0 ||
				uncompressedSize > static_cast<uint64_t>((std::numeric_limits<SIZE_T>::max)()) ||
				compressedSize > static_cast<uint64_t>((std::numeric_limits<SIZE_T>::max)()))
			{
				throw std::runtime_error("Invalid compressed VMDL header.");
			}

			std::vector<uint8_t> compressedData(static_cast<size_t>(compressedSize));
			fileStream.read(
				reinterpret_cast<char*>(compressedData.data()),
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
			const BOOL result = Decompress(
				decompressor,
				compressedData.data(),
				compressedData.size(),
				serializedData.data(),
				serializedData.size(),
				&decompressedSize);
			CloseDecompressor(decompressor);

			if (!result || decompressedSize != serializedData.size())
			{
				throw std::runtime_error("VMDL decompression failed.");
			}

			std::string serializedString(
				reinterpret_cast<const char*>(serializedData.data()),
				serializedData.size());
			std::istringstream serializedStream(serializedString, std::ios::binary | std::ios::in);
			deserialize(serializedStream);
		}
	}
	catch (...)
	{
		_ASSERT_EXPR_A(false, "VMDLModel deserialize failed.");
	}
}
