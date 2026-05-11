// Model.cpp

#include <filesystem>
#include <fstream>
#include <cereal/cereal.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include "Misc.h"
#include "GLTFImporter.h"
#include "GpuResourceUtils.h"
#include "Model.h"
#include <Graphics.h>

const std::vector<D3D11_INPUT_ELEMENT_DESC> Model::InputElementDescs =
{
	{ "POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TANGENT",      0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "BONE_WEIGHTS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "BONE_INDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT,  0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
};

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
		CEREAL_NVP(nodeAnims)
	);
}
#endif

// コンストラクタ
Model::Model(const char* filename, float sampleRate, bool importRawModel)
{
	auto device = Graphics::Instance().GetDevice();

	std::filesystem::path filepath(filename);
	std::filesystem::path dirpath(filepath.parent_path());

	std::filesystem::path extension = filepath.extension();

	// 独自形式のモデルファイルの存在確認
	filepath.replace_extension(".cereal");
	if (std::filesystem::exists(filepath) && !importRawModel)
	{
		// 独自形式のモデルファイルの読み込み
		uint16_t lastWriteTime;
		Deserialize(filepath.string().c_str(), lastWriteTime);
		// cerealが古いなら、元のモデルファイルから再構築する
		if (std::filesystem::exists(filename)) {
			uint16_t fileLastWriteTime = std::filesystem::last_write_time(filename).time_since_epoch().count();
			if (fileLastWriteTime != lastWriteTime)
			{
				Model tmpModel(filename, sampleRate, true);
				*this = std::move(tmpModel);
				return;
			}
		}
	}
	else if (extension == ".gltf" || extension == ".glb")
	{
		// 汎用モデルファイルの読み込み
		GLTFImporter importer(filename);

		// マテリアルデータ読み取り
		importer.LoadMaterials(materials, device);

		// ノードデータ読み取り
		importer.LoadNodes(nodes);

		// メッシュデータ読み取り
		importer.LoadMeshes(meshes, nodes);

		// アニメーションデータ読み取り
		importer.LoadAnimations(animations, nodes, sampleRate);

		// 独自形式のモデルファイルを保存
		//Serialize(filepath.string().c_str(), std::filesystem::last_write_time(filename).time_since_epoch().count());
		// ↑本当は保存できるけど、テクスチャ埋め込めないのが正直キツイのでglbって元々早いしglbでいいです
	}
	else
	{
		_ASSERT_EXPR_A(false, "found not model file");
	}

	// マテリアル構築
	for (Material& material : materials)
	{
		if (material.baseMap == nullptr)
		{
			if (material.baseTextureFileName.empty())
			{
				// ダミーテクスチャ作成
				HRESULT hr = GpuResourceUtils::CreateDummyTexture(device, 0xFFFFFFFF,
					material.baseMap.GetAddressOf());
				_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
			}
			else
			{
				// ベーステクスチャ読み込み
				std::filesystem::path diffuseTexturePath(dirpath / material.baseTextureFileName);
				HRESULT hr = GpuResourceUtils::LoadTexture(device, diffuseTexturePath.string().c_str(),
					material.baseMap.GetAddressOf());
				_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
			}
		}

		if (material.normalMap == nullptr)
		{
			if (material.normalTextureFileName.empty())
			{
				// 法線ダミーテクスチャ作成
				HRESULT hr = GpuResourceUtils::CreateDummyTexture(device, 0xFFFF7F7F,
					material.normalMap.GetAddressOf());
				_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
			}
			else
			{
				// 法線テクスチャ読み込み
				std::filesystem::path texturePath(dirpath / material.normalTextureFileName);
				HRESULT hr = GpuResourceUtils::LoadTexture(device, texturePath.string().c_str(),
					material.normalMap.GetAddressOf());
				_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
			}
		}
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

			HRESULT hr = device->CreateBuffer(&bufferDesc, &subresourceData, mesh.vertexBuffer.GetAddressOf());
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
			HRESULT hr = device->CreateBuffer(&bufferDesc, &subresourceData, mesh.indexBuffer.GetAddressOf());
			_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
		}

		// ボーン構築
		for (Bone& bone : mesh.bones)
		{
			// 参照ノード設定
			bone.node = &nodes.at(bone.nodeIndex);
		}
	}

	// 行列初期化
	DirectX::XMFLOAT4X4 worldTransform;
	DirectX::XMStoreFloat4x4(&worldTransform, DirectX::XMMatrixIdentity());
	UpdateTransform(worldTransform);
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
void Model::UpdateTransform(const DirectX::XMFLOAT4X4& worldTransform)
{
	Matrix ParentWorldTransform = DirectX::XMLoadFloat4x4(&worldTransform);

	for (Node& node : nodes)
	{
		// ローカル行列算出
		Matrix S = Matrix::CreateScale(node.scale);
		Matrix R = Matrix::CreateFromQuaternion(node.rotation);
		Matrix T = Matrix::CreateTranslation(node.position);
		Matrix LocalTransform = S * R * T;

		// グローバル行列算出
		Matrix ParentGlobalTransform;
		if (node.parent != nullptr)
		{
			ParentGlobalTransform = node.parent->globalTransform;
		}
		else
		{
			ParentGlobalTransform = Matrix::Identity;
		}
		Matrix GlobalTransform = LocalTransform * ParentGlobalTransform;

		// ワールド行列算出
		Matrix WorldTransform = GlobalTransform * ParentWorldTransform;

		// 計算結果を格納
		node.localTransform = LocalTransform;
		node.globalTransform = GlobalTransform;
		node.worldTransform = WorldTransform;
	}
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

// シリアライズ
void Model::Serialize(const char* filename, uint16_t lastWrite)
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
void Model::Deserialize(const char* filename, uint16_t& lastWrite)
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
