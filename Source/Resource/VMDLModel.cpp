// VMDLModel.cpp

#include "Resource/VMDLModel.h"
#include "Application/SettingsAndDebug/DebugUtil.h"
#include "Resource/GLTFImporter.h"
#include "Resource/GpuResourceUtils.h"
#include "Rendering/Core/Graphics.h"
#include <algorithm>

template<class Archive>
void VMDLModel::Node::serialize(Archive& archive)
{
	archive(CEREAL_NVP(name), CEREAL_NVP(parentIndex), CEREAL_NVP(position), CEREAL_NVP(rotation), CEREAL_NVP(scale));
}

template<class Archive>
void VMDLModel::Material::serialize(Archive& archive)
{
	archive(
		CEREAL_NVP(name),
		CEREAL_NVP(baseTextureFileName),
		CEREAL_NVP(normalTextureFileName),
		CEREAL_NVP(emissiveTextureFileName),
		CEREAL_NVP(occlusionTextureFileName),
		CEREAL_NVP(metalnessRoughnessTextureFileName),
		CEREAL_NVP(baseTextureDDS),
		CEREAL_NVP(normalTextureDDS),
		CEREAL_NVP(emissiveTextureDDS),
		CEREAL_NVP(occlusionTextureDDS),
		CEREAL_NVP(metalnessRoughnessTextureDDS),
		CEREAL_NVP(baseColor),
		CEREAL_NVP(emissiveColor),
		CEREAL_NVP(metalness),
		CEREAL_NVP(roughness),
		CEREAL_NVP(occlusionStrength),
		CEREAL_NVP(alphaCutoff),
		CEREAL_NVP(alphaMode));
}

template<class Archive>
void VMDLModel::MaterialPbrSettings::serialize(Archive& archive)
{
	archive(CEREAL_NVP(occlusion), CEREAL_NVP(shadowStrength));
}

template<class Archive>
void VMDLModel::Vertex::serialize(Archive& archive)
{
	archive(CEREAL_NVP(position), CEREAL_NVP(boneWeight), CEREAL_NVP(boneIndex), CEREAL_NVP(texcoord), CEREAL_NVP(normal), CEREAL_NVP(tangent));
}

template<class Archive>
void VMDLModel::Bone::serialize(Archive& archive)
{
	archive(CEREAL_NVP(nodeIndex), CEREAL_NVP(offsetTransform));
}

template<class Archive>
void VMDLModel::Mesh::serialize(Archive& archive)
{
	archive(CEREAL_NVP(vertices), CEREAL_NVP(indices), CEREAL_NVP(bones), CEREAL_NVP(nodeIndex), CEREAL_NVP(materialIndex));
}

template<class Archive>
void VMDLModel::VectorKeyframe::serialize(Archive& archive)
{
	archive(CEREAL_NVP(seconds), CEREAL_NVP(value));
}

template<class Archive>
void VMDLModel::QuaternionKeyframe::serialize(Archive& archive)
{
	archive(CEREAL_NVP(seconds), CEREAL_NVP(value));
}

template<class Archive>
void VMDLModel::FootIKRange::serialize(Archive& archive)
{
	archive(
		CEREAL_NVP(name),
		CEREAL_NVP(footIndex),
		CEREAL_NVP(startRatio),
		CEREAL_NVP(endRatio),
		CEREAL_NVP(weight),
		CEREAL_NVP(fadeInRatio),
		CEREAL_NVP(fadeOutRatio));
}

template<class Archive>
void VMDLModel::NodeAnim::serialize(Archive& archive)
{
	archive(CEREAL_NVP(positionKeyframes), CEREAL_NVP(rotationKeyframes), CEREAL_NVP(scaleKeyframes));
}

template<class Archive>
void VMDLModel::Animation::serialize(Archive& archive)
{
	archive(CEREAL_NVP(name), CEREAL_NVP(secondsLength), CEREAL_NVP(nodeAnims), CEREAL_NVP(footIKRanges));
}

template<class Archive>
void VMDLModel::VmdlRigidBody::serialize(Archive& archive)
{
	archive(CEREAL_NVP(name), CEREAL_NVP(nodeIndex), CEREAL_NVP(offsetPosition), CEREAL_NVP(offsetRotation), CEREAL_NVP(mass), CEREAL_NVP(kinematic));
}

template<class Archive>
void VMDLModel::VmdlCollider::serialize(Archive& archive)
{
	archive(CEREAL_NVP(name), CEREAL_NVP(nodeIndex), CEREAL_NVP(shape), CEREAL_NVP(center), CEREAL_NVP(rotation), CEREAL_NVP(size), CEREAL_NVP(trigger));
}

template<class Archive>
void VMDLModel::VmdlSpring::serialize(Archive& archive)
{
	archive(CEREAL_NVP(name), CEREAL_NVP(nodeIndex), CEREAL_NVP(offsetPosition), CEREAL_NVP(offsetRotation), CEREAL_NVP(stiffness), CEREAL_NVP(drag));
}

template<class Archive>
void VMDLModel::VmdlSpringCollider::serialize(Archive& archive)
{
	archive(CEREAL_NVP(name), CEREAL_NVP(nodeIndex), CEREAL_NVP(offsetPosition), CEREAL_NVP(radius));
}

template<class Archive>
void VMDLModel::VmdlShape::serialize(Archive& archive)
{
	archive(CEREAL_NVP(name), CEREAL_NVP(meshVisibility));
}

template<class Archive>
void VMDLModel::VmdlTrail::serialize(Archive& archive)
{
	archive(CEREAL_NVP(name), CEREAL_NVP(nodeIndex), CEREAL_NVP(rootOffset), CEREAL_NVP(tipOffset), CEREAL_NVP(color), CEREAL_NVP(tipRatio), CEREAL_NVP(lifeTime), CEREAL_NVP(maxPoints));
}

template<class Archive>
void VMDLModel::VmdlExtensionData::serialize(Archive& archive)
{
	archive(CEREAL_NVP(rootOffset), CEREAL_NVP(rigidBodies), CEREAL_NVP(colliders), CEREAL_NVP(springs), CEREAL_NVP(springColliders), CEREAL_NVP(shapes));
}

template<class Archive>
void VMDLModel::VmdlIKSettings::serialize(Archive& archive)
{
	archive(
		CEREAL_NVP(type),
		CEREAL_NVP(pelvis),
		CEREAL_NVP(leftThigh),
		CEREAL_NVP(leftCalf),
		CEREAL_NVP(leftFoot),
		CEREAL_NVP(leftBall),
		CEREAL_NVP(rightThigh),
		CEREAL_NVP(rightCalf),
		CEREAL_NVP(rightFoot),
		CEREAL_NVP(rightBall));
}

template<class Archive>
void VMDLModel::VmdlFootWeightTrack::serialize(Archive& archive)
{
	archive(CEREAL_NVP(animationName), CEREAL_NVP(sampleRate), CEREAL_NVP(weights));
}

template<class Archive>
void VMDLModel::VmdlAnimationEditorData::serialize(Archive& archive)
{
	archive(CEREAL_NVP(footWeightTracks));
}

template<class Archive>
void VMDLModel::VmdlBoolKeyframe::serialize(Archive& archive)
{
	archive(CEREAL_NVP(seconds), CEREAL_NVP(value));
}

template<class Archive>
void VMDLModel::VmdlColliderAnimationTrack::serialize(Archive& archive)
{
	archive(CEREAL_NVP(animationName), CEREAL_NVP(colliderIndex), CEREAL_NVP(keys));
}

template<class Archive>
void VMDLModel::VmdlShapeKeyframe::serialize(Archive& archive)
{
	archive(CEREAL_NVP(seconds), CEREAL_NVP(shapeIndex));
}

template<class Archive>
void VMDLModel::VmdlShapeAnimationTrack::serialize(Archive& archive)
{
	archive(CEREAL_NVP(animationName), CEREAL_NVP(keys));
}

template<class Archive>
void VMDLModel::VmdlTrailAnimationTrack::serialize(Archive& archive)
{
	archive(CEREAL_NVP(animationName), CEREAL_NVP(trailIndex), CEREAL_NVP(keys));
}

template<class Archive>
void VMDLModel::VmdlTrailData::serialize(Archive& archive)
{
	archive(CEREAL_NVP(trails), CEREAL_NVP(initialActive), CEREAL_NVP(tracks));
}

template<class Archive>
void VMDLModel::VmdlPlacementData::serialize(Archive& archive)
{
	archive(CEREAL_NVP(scale), CEREAL_NVP(initialized));
}

template<class Archive>
void VMDLModel::VmdlAnimationControlData::serialize(Archive& archive)
{
	archive(CEREAL_NVP(colliderInitialActive), CEREAL_NVP(colliderTracks), CEREAL_NVP(shapeTracks));
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

HRESULT VMDLModel::SaveScratchImageToDDSBytes(
	const DirectX::ScratchImage& sourceImage,
	std::vector<uint8_t>& outDDS)
{
	outDDS.clear();

	constexpr size_t MaxEmbeddedTextureSize = 2048;
	const DirectX::TexMetadata& sourceMetadata = sourceImage.GetMetadata();

	// DDSへ統一する前にRGBA8へ変換する。埋め込みサイズを抑えるため長辺を2048pxに制限し、
	// 縮小後の画像からミップを生成する。ミップ生成に失敗しても元画像は保存できる。
	DirectX::ScratchImage rgbaImage;
	HRESULT hr = S_OK;

	if (sourceMetadata.format != DXGI_FORMAT_R8G8B8A8_UNORM)
	{
		hr = DirectX::Convert(
			sourceImage.GetImages(),
			sourceImage.GetImageCount(),
			sourceMetadata,
			DXGI_FORMAT_R8G8B8A8_UNORM,
			DirectX::TEX_FILTER_DEFAULT,
			DirectX::TEX_THRESHOLD_DEFAULT,
			rgbaImage
		);

		if (FAILED(hr))
		{
			return hr;
		}
	}
	else
	{
		hr = rgbaImage.InitializeFromImage(*sourceImage.GetImage(0, 0, 0));

		if (FAILED(hr))
		{
			return hr;
		}
	}

	const DirectX::ScratchImage* mipSource = &rgbaImage;

	DirectX::ScratchImage resizedImage;
	const DirectX::TexMetadata& rgbaMetadata = rgbaImage.GetMetadata();
	const size_t maxSize = (std::max)(rgbaMetadata.width, rgbaMetadata.height);
	if (maxSize > MaxEmbeddedTextureSize)
	{
		const double scale = static_cast<double>(MaxEmbeddedTextureSize) / static_cast<double>(maxSize);
		const size_t resizedWidth = (std::max)(static_cast<size_t>(1), static_cast<size_t>(rgbaMetadata.width * scale));
		const size_t resizedHeight = (std::max)(static_cast<size_t>(1), static_cast<size_t>(rgbaMetadata.height * scale));

		hr = DirectX::Resize(
			rgbaImage.GetImages(),
			rgbaImage.GetImageCount(),
			rgbaMetadata,
			resizedWidth,
			resizedHeight,
			DirectX::TEX_FILTER_DEFAULT,
			resizedImage
		);

		if (FAILED(hr))
		{
			return hr;
		}

		mipSource = &resizedImage;
	}

	const DirectX::ScratchImage* saveSource = mipSource;

	DirectX::ScratchImage mipImage;
	hr = DirectX::GenerateMipMaps(
		mipSource->GetImages(),
		mipSource->GetImageCount(),
		mipSource->GetMetadata(),
		DirectX::TEX_FILTER_DEFAULT,
		0,
		mipImage
	);

	if (SUCCEEDED(hr))
	{
		saveSource = &mipImage;
	}

	DirectX::Blob blob;
	hr = DirectX::SaveToDDSMemory(
		saveSource->GetImages(),
		saveSource->GetImageCount(),
		saveSource->GetMetadata(),
		DirectX::DDS_FLAGS_NONE,
		blob
	);

	if (FAILED(hr))
	{
		return hr;
	}

	outDDS.resize(blob.GetBufferSize());
	memcpy(outDDS.data(), blob.GetBufferPointer(), blob.GetBufferSize());

	return S_OK;
}

HRESULT VMDLModel::ConvertTextureFileToDDSBytes(
	const std::filesystem::path& texturePath,
	std::vector<uint8_t>& outDDS)
{
	outDDS.clear();

	if (!std::filesystem::exists(texturePath))
	{
		return E_FAIL;
	}

	std::wstring extension = ToLowerWString(texturePath.extension().wstring());

	if (extension == L".dds")
	{
		return ReadBinaryFile(texturePath, outDDS) ? S_OK : E_FAIL;
	}

	DirectX::ScratchImage image;
	DirectX::TexMetadata metadata = {};
	HRESULT hr = E_FAIL;

	if (extension == L".tga")
	{
		hr = DirectX::LoadFromTGAFile(
			texturePath.wstring().c_str(),
			&metadata,
			image
		);
	}
	else
	{
		hr = DirectX::LoadFromWICFile(
			texturePath.wstring().c_str(),
			DirectX::WIC_FLAGS_FORCE_RGB,
			&metadata,
			image
		);
	}

	if (FAILED(hr))
	{
		return hr;
	}

	return SaveScratchImageToDDSBytes(image, outDDS);
}

HRESULT VMDLModel::ConvertSRVToDDSBytes(
	ID3D11Device* device,
	ID3D11ShaderResourceView* srv,
	std::vector<uint8_t>& outDDS)
{
	outDDS.clear();

	if (device == nullptr || srv == nullptr)
	{
		return E_FAIL;
	}

	Microsoft::WRL::ComPtr<ID3D11Resource> resource;
	srv->GetResource(resource.GetAddressOf());

	if (resource == nullptr)
	{
		return E_FAIL;
	}

	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
	device->GetImmediateContext(context.GetAddressOf());

	if (context == nullptr)
	{
		return E_FAIL;
	}

	DirectX::ScratchImage image;
	HRESULT hr = DirectX::CaptureTexture(
		device,
		context.Get(),
		resource.Get(),
		image
	);

	if (FAILED(hr))
	{
		return hr;
	}

	return SaveScratchImageToDDSBytes(image, outDDS);
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
	CaptureRuntimeShapeVisibility();
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
	vmdlPlacementData(other.vmdlPlacementData),
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
	vmdlPlacementData(std::move(other.vmdlPlacementData)),
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
	vmdlPlacementData = other.vmdlPlacementData;
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
	vmdlPlacementData = std::move(other.vmdlPlacementData);
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
	CaptureRuntimeShapeVisibility();
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
	for (size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex)
	{
		if (nodes.at(nodeIndex).name == name)
		{
			return static_cast<int>(nodeIndex);
		}
	}
	return -1;
}

int VMDLModel::GetShapeIndex(const char* name) const
{
	if (!name) return -1;
	const auto& shapes = vmdlExtensionData.shapes;
	for (int i = 0; i < static_cast<int>(shapes.size()); ++i)
	{
		if (shapes[i].name == name) return i;
	}
	return -1;
}

bool VMDLModel::ApplyShape(const char* name)
{
	return ApplyShape(GetShapeIndex(name));
}

bool VMDLModel::ApplyShape(int shapeIndex)
{
	const auto& shapes = vmdlExtensionData.shapes;
	if (shapeIndex < 0 || shapeIndex >= static_cast<int>(shapes.size())) return false;
	if (runtimeShapeVisibility.size() != meshes.size()) CaptureRuntimeShapeVisibility();

	const auto& visibility = shapes[shapeIndex].meshVisibility;
	const size_t count = std::min(meshes.size(), visibility.size());
	for (size_t i = 0; i < count; ++i)
	{
		if (visibility[i] == 1) runtimeShapeVisibility[i] = 1;
		else if (visibility[i] == 0) runtimeShapeVisibility[i] = 0;
	}
	return ApplyShapeToMeshes(shapeIndex);
}

void VMDLModel::UpdateTransform(const Matrix& worldTransform)
{
	for (Node& node : nodes)
	{
		if (node.parent == nullptr)
		{
			UpdateNodeTransform(node, Matrix::Identity, worldTransform);
		}
	}
}

const Matrix& VMDLModel::GetWorldTransform() const
{
	for (const Node& node : nodes)
	{
		if (node.parent == nullptr)
		{
			return node.worldTransform;
		}
	}

	static const Matrix identity = Matrix::Identity;
	return identity;
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

VMDLModel::VmdlShapeAnimationTrack& VMDLModel::GetOrCreateShapeAnimationTrack(const std::string& animationName)
{
	for (auto& track : vmdlAnimationControlData.shapeTracks)
	{
		if (track.animationName == animationName) return track;
	}
	auto& track = vmdlAnimationControlData.shapeTracks.emplace_back();
	track.animationName = animationName;
	return track;
}

const VMDLModel::VmdlShapeAnimationTrack* VMDLModel::FindShapeAnimationTrack(const std::string& animationName) const
{
	for (const auto& track : vmdlAnimationControlData.shapeTracks)
	{
		if (track.animationName == animationName) return &track;
	}
	return nullptr;
}

void VMDLModel::CaptureRuntimeShapeVisibility()
{
	runtimeShapeVisibility.clear();
	runtimeShapeVisibility.reserve(meshes.size());
	for (const Mesh& mesh : meshes) runtimeShapeVisibility.push_back(mesh.isDraw ? 1 : 0);
}

bool VMDLModel::ApplyShapeToMeshes(int shapeIndex)
{
	const auto& shapes = vmdlExtensionData.shapes;
	if (shapeIndex < 0 || shapeIndex >= static_cast<int>(shapes.size())) return false;

	const auto& visibility = shapes[shapeIndex].meshVisibility;
	const size_t count = std::min(meshes.size(), visibility.size());
	for (size_t i = 0; i < count; ++i)
	{
		if (visibility[i] == 1) meshes[i].isDraw = true;
		else if (visibility[i] == 0) meshes[i].isDraw = false;
	}
	return true;
}

void VMDLModel::RestoreShapeVisibility(const std::vector<uint8_t>& visibility)
{
	const size_t count = std::min(meshes.size(), visibility.size());
	for (size_t i = 0; i < count; ++i) meshes[i].isDraw = visibility[i] != 0;
	CaptureRuntimeShapeVisibility();
}

void VMDLModel::RestoreRuntimeShapeVisibility()
{
	const size_t count = std::min(meshes.size(), runtimeShapeVisibility.size());
	for (size_t i = 0; i < count; ++i) meshes[i].isDraw = runtimeShapeVisibility[i] != 0;
}

void VMDLModel::ApplyShapeAnimation(int animationIndex, float time)
{
	RestoreRuntimeShapeVisibility();
	if (animationIndex < 0 || animationIndex >= static_cast<int>(animations.size())) return;

	const Animation& animation = animations[animationIndex];
	if (animation.secondsLength > 0.0f)
	{
		while (time < 0.0f) time += animation.secondsLength;
		while (time > animation.secondsLength) time -= animation.secondsLength;
	}

	const auto* track = FindShapeAnimationTrack(animation.name);
	if (!track) return;
	// Shapeは差分指定なので、現在時刻までのキーを先頭から順に適用して結果を再現する。
	for (const auto& key : track->keys)
	{
		if (key.seconds > time) break;
		ApplyShapeToMeshes(key.shapeIndex);
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
	std::ofstream ostream(filename, std::ios::binary);
	if (ostream.is_open())
	{
		cereal::BinaryOutputArchive archive(ostream);
		std::vector<MaterialPbrSettings> materialPbrSettings;
		materialPbrSettings.reserve(materials.size());
		for (const Material& material : materials)
			materialPbrSettings.push_back({material.occlusion, material.shadowStrength});

		try
		{
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
				CEREAL_NVP(vmdlPlacementData)
			);
		}
		catch (...)
		{
			_ASSERT_EXPR_A(false, "VMDLModel serialize failed.");
		}
	}
}

void VMDLModel::Deserialize(const char* filename, uint64_t& lastWrite)
{
	std::ifstream istream(filename, std::ios::binary);
	if (istream.is_open())
	{
		cereal::BinaryInputArchive archive(istream);

		try
		{
			archive(
				CEREAL_NVP(lastWrite),
				CEREAL_NVP(nodes),
				CEREAL_NVP(materials),
				CEREAL_NVP(meshes),
				CEREAL_NVP(animations)
			);

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
				archive(CEREAL_NVP(vmdlPlacementData));
			}
			catch (...)
			{
				vmdlPlacementData = {};
			}
		}
		catch (...)
		{
			_ASSERT_EXPR_A(false, "VMDLModel deserialize failed.");
		}
	}
	else
	{
		_ASSERT_EXPR_A(false, "VMDLModel File not found.");
	}
}

