// Model.cpp

#include "Model.h"
#include "DebugUtil.h"
#include "GLTFImporter.h"
#include "GpuResourceUtils.h"
#include "Graphics.h"
#include <cereal/cereal.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/utility.hpp>
#include <algorithm>

#if 1 // シリアライズ閉じる用
namespace DirectX
{
	template<class Archive>
	void serialize(Archive& archive, XMUINT4& v)
	{
		archive(
			cereal::make_nvp("x", v.x),
			cereal::make_nvp("y", v.y),
			cereal::make_nvp("z", v.z),
			cereal::make_nvp("w", v.w)
		);
	}

	template<class Archive>
	void serialize(Archive& archive, XMFLOAT2& v)
	{
		archive(
			cereal::make_nvp("x", v.x),
			cereal::make_nvp("y", v.y)
		);
	}

	template<class Archive>
	void serialize(Archive& archive, XMFLOAT3& v)
	{
		archive(
			cereal::make_nvp("x", v.x),
			cereal::make_nvp("y", v.y),
			cereal::make_nvp("z", v.z)
		);
	}

	template<class Archive>
	void serialize(Archive& archive, XMFLOAT4& v)
	{
		archive(
			cereal::make_nvp("x", v.x),
			cereal::make_nvp("y", v.y),
			cereal::make_nvp("z", v.z),
			cereal::make_nvp("w", v.w)
		);
	}

	template<class Archive>
	void serialize(Archive& archive, XMFLOAT4X4& m)
	{
		archive(
			cereal::make_nvp("_11", m._11),
			cereal::make_nvp("_12", m._12),
			cereal::make_nvp("_13", m._13),
			cereal::make_nvp("_14", m._14),
			cereal::make_nvp("_21", m._21),
			cereal::make_nvp("_22", m._22),
			cereal::make_nvp("_23", m._23),
			cereal::make_nvp("_24", m._24),
			cereal::make_nvp("_31", m._31),
			cereal::make_nvp("_32", m._32),
			cereal::make_nvp("_33", m._33),
			cereal::make_nvp("_34", m._34),
			cereal::make_nvp("_41", m._41),
			cereal::make_nvp("_42", m._42),
			cereal::make_nvp("_43", m._43),
			cereal::make_nvp("_44", m._44)
		);
	}
}

template<class Archive>
void Model::Node::serialize(Archive& archive)
{
	archive(
		CEREAL_NVP(name),
		CEREAL_NVP(parentIndex),
		CEREAL_NVP(position),
		CEREAL_NVP(rotation),
		CEREAL_NVP(scale)
	);
}

template<class Archive>
void Model::Material::serialize(Archive& archive)
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
		CEREAL_NVP(alphaMode)
	);
}

template<class Archive>
void Model::Vertex::serialize(Archive& archive)
{
	archive(
		CEREAL_NVP(position),
		CEREAL_NVP(boneWeight),
		CEREAL_NVP(boneIndex),
		CEREAL_NVP(texcoord),
		CEREAL_NVP(normal),
		CEREAL_NVP(tangent)
	);
}

template<class Archive>
void Model::Bone::serialize(Archive& archive)
{
	archive(
		CEREAL_NVP(nodeIndex),
		CEREAL_NVP(offsetTransform)
	);
}

template<class Archive>
void Model::Mesh::serialize(Archive& archive)
{
	archive(
		CEREAL_NVP(vertices),
		CEREAL_NVP(indices),
		CEREAL_NVP(bones),
		CEREAL_NVP(nodeIndex),
		CEREAL_NVP(materialIndex)
	);
}

template<class Archive>
void Model::VectorKeyframe::serialize(Archive& archive)
{
	archive(
		CEREAL_NVP(seconds),
		CEREAL_NVP(value)
	);
}

template<class Archive>
void Model::QuaternionKeyframe::serialize(Archive& archive)
{
	archive(
		CEREAL_NVP(seconds),
		CEREAL_NVP(value)
	);
}

template<class Archive>
void Model::FootIKRange::serialize(Archive& archive)
{
	archive(
		CEREAL_NVP(name),
		CEREAL_NVP(footIndex),
		CEREAL_NVP(startRatio),
		CEREAL_NVP(endRatio),
		CEREAL_NVP(weight),
		CEREAL_NVP(fadeInRatio),
		CEREAL_NVP(fadeOutRatio)
	);
}
template<class Archive>
void Model::NodeAnim::serialize(Archive& archive)
{
	archive(
		CEREAL_NVP(positionKeyframes),
		CEREAL_NVP(rotationKeyframes),
		CEREAL_NVP(scaleKeyframes)
	);
}

template<class Archive>
void Model::Animation::serialize(Archive& archive)
{
	archive(
		CEREAL_NVP(name),
		CEREAL_NVP(secondsLength),
		CEREAL_NVP(nodeAnims),
		CEREAL_NVP(footIKRanges)
	);
}
#endif

namespace
{
	constexpr uint64_t ModelCacheVersion = 3;

	uint64_t MakeModelCacheStamp(uint64_t sourceLastWrite)
	{
		return sourceLastWrite ^ ModelCacheVersion;
	}

	bool ReadModelCacheStamp(const std::filesystem::path& filepath, uint64_t& stamp)
	{
		std::ifstream stream(filepath, std::ios::binary);
		if (!stream.is_open()) return false;

		stream.read(reinterpret_cast<char*>(&stamp), sizeof(stamp));
		return stream.good();
	}
}

uint64_t Model::GetFileLastWriteTime64(const std::filesystem::path& path)
{
	return static_cast<uint64_t>(
		std::filesystem::last_write_time(path).time_since_epoch().count()
		);
}

std::wstring Model::ToLowerWString(std::wstring text)
{
	std::transform(
		text.begin(),
		text.end(),
		text.begin(),
		[](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); }
	);

	return text;
}

bool Model::ReadBinaryFile(const std::filesystem::path& path, std::vector<uint8_t>& outData)
{
	outData.clear();

	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file)
	{
		return false;
	}

	std::streamsize size = file.tellg();
	if (size <= 0)
	{
		return false;
	}

	outData.resize(static_cast<size_t>(size));

	file.seekg(0, std::ios::beg);
	file.read(reinterpret_cast<char*>(outData.data()), size);

	if (!file)
	{
		outData.clear();
		return false;
	}

	return true;
}

HRESULT Model::SaveScratchImageToDDSBytes(
	const DirectX::ScratchImage& sourceImage,
	std::vector<uint8_t>& outDDS)
{
	outDDS.clear();

	constexpr size_t MaxEmbeddedTextureSize = 2048;
	const DirectX::TexMetadata& sourceMetadata = sourceImage.GetMetadata();

	DirectX::ScratchImage rgbaImage;
	HRESULT hr = S_OK;

	// DDSとして扱いやすいRGBA8へ変換する
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

	// ミップマップ生成
	// 軽さ最優先ならここも消していい
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

	// 圧縮せずDDSメモリへ保存
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

HRESULT Model::ConvertTextureFileToDDSBytes(
	const std::filesystem::path& texturePath,
	std::vector<uint8_t>& outDDS)
{
	outDDS.clear();

	if (!std::filesystem::exists(texturePath))
	{
		return E_FAIL;
	}

	std::wstring extension = ToLowerWString(texturePath.extension().wstring());

	// 元がDDSなら再圧縮せず、そのまま埋め込む
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

HRESULT Model::ConvertSRVToDDSBytes(
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

void Model::BuildEmbeddedDDSFromFileOrSRV(
	ID3D11Device* device,
	const std::filesystem::path& dirpath,
	const std::string& textureFileName,
	ID3D11ShaderResourceView* srv,
	std::vector<uint8_t>& outDDS)
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

	// glb内蔵テクスチャなど、ファイル名が取れない場合の保険。
	// importer.LoadMaterials がSRVを作っているなら、GPUから捕まえてDDS化する。
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

void Model::BuildMaterialEmbeddedDDS(
	ID3D11Device* device,
	const std::filesystem::path& dirpath,
	Model::Material& material)
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

void Model::CreateSRVFromEmbeddedDDSOrFile(
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

void Model::BuildMaterialTextureResources(
	ID3D11Device* device,
	const std::filesystem::path& dirpath,
	Model::Material& material)
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

Model::Model(const char* filename, float sampleRate, bool importRawModel)
{
	auto device = Game::Graphics::Instance().GetDevice();

	std::filesystem::path sourceFilepath(filename);
	std::filesystem::path dirpath(sourceFilepath.parent_path());
	std::filesystem::path extension = sourceFilepath.extension();

	std::filesystem::path cerealFilepath = sourceFilepath;
	cerealFilepath.replace_extension(".vmdl");
	modelCacheFilepath = cerealFilepath;
	modelCacheLastWrite = std::filesystem::exists(filename)
		? MakeModelCacheStamp(GetFileLastWriteTime64(filename))
		: 0;

	// 独自形式のモデルファイルの存在確認
	if (std::filesystem::exists(cerealFilepath) && !importRawModel)
	{
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
			Model tmpModel(filename, sampleRate, true);
			*this = std::move(tmpModel);
			return;
		}

		uint64_t lastWriteTime = 0;
		Deserialize(cerealFilepath.string().c_str(), lastWriteTime);
		modelCacheLastWrite = lastWriteTime;
	}
	else if (extension == ".gltf" || extension == ".glb")
	{
		// 汎用モデルファイルの読み込み
		GLTFImporter importer(filename);

		// マテリアルデータ読み取り
		// GLB内蔵テクスチャは一度ファイル化してからDDS化する。
		// 巨大テクスチャをGPUからCaptureTextureするとDebug Layerで落ちやすいため。
		importer.LoadMaterials(materials, nullptr);

		// ノードデータ読み取り
		importer.LoadNodes(nodes);

		// メッシュデータ読み取り
		importer.LoadMeshes(meshes, nodes);

		// アニメーションデータ読み取り
		importer.LoadAnimations(animations, nodes, sampleRate);

		// テクスチャをDDS化してcerealに埋め込む
		for (Material& material : materials)
		{
			BuildMaterialEmbeddedDDS(device, dirpath, material);
		}

		// 独自形式のモデルファイルを保存
		Serialize(
			cerealFilepath.string().c_str(),
			MakeModelCacheStamp(GetFileLastWriteTime64(filename))
		);
	}
	else
	{
		_ASSERT_EXPR_A(false, "found not model file");
	}

	// マテリアル構築
	for (Material& material : materials)
	{
		BuildMaterialTextureResources(device, dirpath, material);
	}

	// ノード構築
	for (size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex)
	{
		Node& node = nodes.at(nodeIndex);

		// 親子関係を構築
		node.parent = node.parentIndex >= 0 ? &nodes.at(node.parentIndex) : nullptr;

		if (node.parent != nullptr)
		{
			node.parent->children.emplace_back(&node);
		}
	}

	// メッシュ構築
	for (Mesh& mesh : meshes)
	{
		// 参照マテリアル設定
		mesh.material = &materials.at(mesh.materialIndex);

		// 参照ノード設定
		mesh.node = &nodes.at(mesh.nodeIndex);

		// 頂点バッファ
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

		// インデックスバッファ
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

		// ボーン構築
		for (Bone& bone : mesh.bones)
		{
			bone.node = &nodes.at(bone.nodeIndex);
		}
	}

	// 行列初期化
	UpdateTransform(Matrix::Identity);
}

// アニメーション追加読み込み
void Model::AppendAnimations(const char* filename)
{
	std::filesystem::path filepath(filename);

	if (filepath.extension() == ".gltf" ||
		filepath.extension() == ".glb")
	{
		GLTFImporter importer(filename);

		// アニメーションファイル側のノードを取得
		std::vector<Node> animNodes;
		importer.LoadNodes(animNodes);

		// アニメーションをアニメファイルのノード基準で読み込む
		std::vector<Animation> newAnims;
		importer.LoadAnimations(newAnims, animNodes);

		// モデル側のノード名→インデックスのマップ作成
		std::unordered_map<std::string, int> modelNodeMap;
		for (int i = 0; i < (int)nodes.size(); ++i)
			modelNodeMap[nodes[i].name] = i;

		// nodeAnimsをモデルのノード順に並べ替える
		for (Animation& anim : newAnims)
		{
			Animation remapped;
			//remapped.name = anim.name;
			remapped.name = filepath.stem().string();
			remapped.secondsLength = anim.secondsLength;
			remapped.nodeAnims.resize(nodes.size());

			// まずモデルのノードの初期姿勢で初期化
			for (int i = 0; i < (int)nodes.size(); ++i)
			{
				Model::VectorKeyframe pk;
				pk.seconds = 0.0f;
				pk.value = nodes[i].position;
				remapped.nodeAnims[i].positionKeyframes.push_back(pk);
				pk.seconds = anim.secondsLength;
				remapped.nodeAnims[i].positionKeyframes.push_back(pk);

				Model::QuaternionKeyframe rk;
				rk.seconds = 0.0f;
				rk.value = nodes[i].rotation;
				remapped.nodeAnims[i].rotationKeyframes.push_back(rk);
				rk.seconds = anim.secondsLength;
				remapped.nodeAnims[i].rotationKeyframes.push_back(rk);

				Model::VectorKeyframe sk;
				sk.seconds = 0.0f;
				sk.value = nodes[i].scale;
				remapped.nodeAnims[i].scaleKeyframes.push_back(sk);
				sk.seconds = anim.secondsLength;
				remapped.nodeAnims[i].scaleKeyframes.push_back(sk);
			}

			// アニメーションノードをモデルのノードインデックスに対応付け
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

// アニメーションインデックス取得
int Model::GetAnimationIndex(const char* name) const
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

static void UpdateNodeTransform(Model::Node& node, const Matrix& parentGlobal, const Matrix& worldTransform)
{
	Matrix S = Matrix::CreateScale(node.scale);
	Matrix R = Matrix::CreateFromQuaternion(node.rotation);
	Matrix T = Matrix::CreateTranslation(node.position);
	Matrix localTransform = S * R * T;
	Matrix globalTransform = localTransform * parentGlobal;

	node.localTransform = localTransform;
	node.globalTransform = globalTransform;
	node.worldTransform = globalTransform * worldTransform;

	for (Model::Node* child : node.children)
	{
		UpdateNodeTransform(*child, globalTransform, worldTransform);
	}
}

// ノードインデックス取得
int Model::GetNodeIndex(const char* name) const
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

// トランスフォーム更新処理
void Model::UpdateTransform(const Matrix& worldTransform)
{
	for (Node& node : nodes)
	{
		if (node.parent == nullptr)
		{
			UpdateNodeTransform(node, Matrix::Identity, worldTransform);
		}
	}
}

const Matrix& Model::GetWorldTransform() const
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

void Model::ComputeAnimation(int animationIndex, int nodeIndex, float time, NodePose& nodePose) const
{
	const Animation& animation = animations.at(animationIndex);
	const NodeAnim& nodeAnim = animation.nodeAnims.at(nodeIndex);

	// 位置
	for (size_t index = 0; index < nodeAnim.positionKeyframes.size() - 1; ++index)
	{
		// 現在の時間がどのキーフレームの間にいるか判定する
		const VectorKeyframe& keyframe0 = nodeAnim.positionKeyframes.at(index);
		const VectorKeyframe& keyframe1 = nodeAnim.positionKeyframes.at(index + 1);
		if (time >= keyframe0.seconds && time <= keyframe1.seconds)
		{
			// 再生時間とキーフレームの時間から補完率を算出する
			float rate = (time - keyframe0.seconds) / (keyframe1.seconds - keyframe0.seconds);

			// 前のキーフレームと次のキーフレームの姿勢を補完し、計算結果をノードに格納
			nodePose.position = Vector3::Lerp(keyframe0.value, keyframe1.value, rate);
		}
	}
	// 回転
	for (size_t index = 0; index < nodeAnim.rotationKeyframes.size() - 1; ++index)
	{
		// 現在の時間がどのキーフレームの間にいるか判定する
		const QuaternionKeyframe& keyframe0 = nodeAnim.rotationKeyframes.at(index);
		const QuaternionKeyframe& keyframe1 = nodeAnim.rotationKeyframes.at(index + 1);
		if (time >= keyframe0.seconds && time <= keyframe1.seconds)
		{
			// 再生時間とキーフレームの時間から補完率を算出する
			float rate = (time - keyframe0.seconds) / (keyframe1.seconds - keyframe0.seconds);

			// 前のキーフレームと次のキーフレームの姿勢を補完し、計算結果をノードに格納
			nodePose.rotation = Quaternion::Slerp(keyframe0.value, keyframe1.value, rate);
		}
	}
	// スケール
	for (size_t index = 0; index < nodeAnim.scaleKeyframes.size() - 1; ++index)
	{
		// 現在の時間がどのキーフレームの間にいるか判定する
		const VectorKeyframe& keyframe0 = nodeAnim.scaleKeyframes.at(index);
		const VectorKeyframe& keyframe1 = nodeAnim.scaleKeyframes.at(index + 1);
		if (time >= keyframe0.seconds && time <= keyframe1.seconds)
		{
			// 再生時間とキーフレームの時間から補完率を算出する
			float rate = (time - keyframe0.seconds) / (keyframe1.seconds - keyframe0.seconds);

			// 前のキーフレームと次のキーフレームの姿勢を補完し、計算結果をノードに格納
			nodePose.scale = Vector3::Lerp(keyframe0.value, keyframe1.value, rate);
		}
	}
}

// アニメーション計算
void Model::ComputeAnimation(int animationIndex, float time, std::vector<NodePose>& nodePoses) const
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

// ノードポーズ設定
float Model::EvaluateFootIKWeight(int animationIndex, float time, int footIndex) const
{
	if (animationIndex < 0) return 0.0f;
	if (animationIndex >= static_cast<int>(animations.size())) return 0.0f;

	const Animation& animation = animations[animationIndex];
	if (animation.secondsLength > 0.0f)
	{
		while (time < 0.0f) time += animation.secondsLength;
		while (time > animation.secondsLength) time -= animation.secondsLength;
	}

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

bool Model::SaveVmdl()
{
	if (modelCacheFilepath.empty()) return false;

	Serialize(
		modelCacheFilepath.string().c_str(),
		modelCacheLastWrite);
	return true;
}
void Model::SetNodePoses(const std::vector<NodePose>& nodePoses)
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

// ノードポーズ取得
void Model::GetNodePoses(std::vector<NodePose>& nodePoses) const
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

void Model::_print() const
{
	char buf[512];

	printf("========== Model::_print ==========\n");

	// --- ノード一覧 ---
	printf("[Nodes] count=%zu\n", nodes.size());
	for (int i = 0; i < static_cast<int>(nodes.size()); ++i)
	{
		const Node& n = nodes[i];
		snprintf(buf, sizeof(buf),
			"  [%2d] name=%-40s parentIndex=%2d\n",
			i, n.name.c_str(), n.parentIndex);
		printf("%s", buf);
		OutputDebugStringA(buf);
	}

	// --- ボーン一覧（メッシュごと）---
	printf("[Meshes] count=%zu\n", meshes.size());
	for (int mi = 0; mi < static_cast<int>(meshes.size()); ++mi)
	{
		const Mesh& mesh = meshes[mi];
		snprintf(buf, sizeof(buf),
			"  Mesh[%d] nodeIndex=%d  bones=%zu\n",
			mi, mesh.nodeIndex, mesh.bones.size());
		printf("%s", buf);
		OutputDebugStringA(buf);

		for (int bi = 0; bi < static_cast<int>(mesh.bones.size()); ++bi)
		{
			const Bone& bone = mesh.bones[bi];
			const char* boneName = (bone.nodeIndex >= 0 && bone.nodeIndex < static_cast<int>(nodes.size()))
				? nodes[bone.nodeIndex].name.c_str()
				: "(invalid)";
			snprintf(buf, sizeof(buf),
				"    Bone[%2d] nodeIndex=%2d  nodeName=%s\n",
				bi, bone.nodeIndex, boneName);
			printf("%s", buf);
			OutputDebugStringA(buf);
		}
	}

	// --- アニメーション一覧 ---
	printf("[Animations] count=%zu\n", animations.size());
	for (int i = 0; i < static_cast<int>(animations.size()); ++i)
	{
		snprintf(buf, sizeof(buf),
			"  [%2d] name=%-30s  length=%.3fs\n",
			i, animations[i].name.c_str(), animations[i].secondsLength);
		printf("%s", buf);
		OutputDebugStringA(buf);
	}

	printf("====================================\n");
}

// シリアライズ
void Model::Serialize(const char* filename, uint64_t lastWrite)
{
	std::ofstream ostream(filename, std::ios::binary);
	if (ostream.is_open())
	{
		cereal::BinaryOutputArchive archive(ostream);

		try
		{
			archive(
				CEREAL_NVP(lastWrite),
				CEREAL_NVP(nodes),
				CEREAL_NVP(materials),
				CEREAL_NVP(meshes),
				CEREAL_NVP(animations)
			);
		}
		catch (...)
		{
			_ASSERT_EXPR_A(false, "Model serialize failed.");
		}
	}
}

// デシリアライズ
void Model::Deserialize(const char* filename, uint64_t& lastWrite)
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
		}
		catch (...)
		{
			_ASSERT_EXPR_A(false, "Model deserialize failed.");
		}
	}
	else
	{
		_ASSERT_EXPR_A(false, "Model File not found.");
	}
}





