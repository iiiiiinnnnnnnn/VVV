// Model.h

#pragma once

#include "Common.h"
#include "ShaderParam.h"
#include "RenderContext.h"

// Cereal
#include <cereal/cereal.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

class Model
{
public:
	Model(const char* filename, float sampleRate = 60, bool importRawModel = false);

	struct Node
	{
		std::string			name;
		int					parentIndex = -1;
		Vector3				position = Vector3::Zero;
		Quaternion			rotation = Quaternion::Identity;
		Vector3				scale = Vector3::One;

		Matrix				localTransform;
		Matrix				globalTransform;
		Matrix				worldTransform;

		Node*				parent = nullptr; 
		std::vector<Node*>	children;

		template<class Archive>
		void serialize(Archive& archive);
	};

	enum class AlphaMode
	{
		Opaque,
		Mask,
		Blend
	};

	struct Material
	{
		// 保存する
		std::string			name;
		std::string			baseTextureFileName;
		std::string			normalTextureFileName;
		std::string			emissiveTextureFileName;
		std::string			occlusionTextureFileName;
		std::string			metalnessRoughnessTextureFileName;
		Color				baseColor = { 1, 1, 1, 1 };
		Color				emissiveColor = { 1, 1, 1, 1 };
		float				metalness = 0.0f;
		float				roughness = 0.0f;
		float				occlusionStrength = 0.0f;
		float				alphaCutoff = 0.5f;
		AlphaMode			alphaMode = AlphaMode::Opaque;
		ShaderParamList		shaderParams = {}; // カスタムパラメータ

		// 保存しない
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	baseMap;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	normalMap;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	emissiveMap;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	occlusionMap;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	metalnessRoughnessMap;

		template<class Archive>
		void serialize(Archive& archive);
	};

	struct Vertex
	{
		Vector3				position = Vector3::Zero;
		Vector3				normal = Vector3::Zero;
		Vector4				tangent = { 0, 0, 0, 1 };
		Vector2				texcoord = { 0, 0 };
		Vector4				boneWeight = { 1, 0, 0, 0 };
		DirectX::XMUINT4	boneIndex = { 0, 0, 0, 0 };

		template<class Archive>
		void serialize(Archive& archive);
	};

	struct Bone
	{
		int					nodeIndex;
		Matrix				offsetTransform;
		Node*				node = nullptr;

		template<class Archive>
		void serialize(Archive& archive);
	};

	struct Mesh
	{
		// 保存する
		std::vector<Vertex>		vertices;
		std::vector<uint32_t>	indices;
		std::vector<Bone>		bones;
		int			nodeIndex = 0;
		int			materialIndex = 0;

		// 保存しない
		Material*	material = nullptr;
		Node*		node = nullptr;
		bool		isDraw = true;
		Microsoft::WRL::ComPtr<ID3D11Buffer>	vertexBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer>	indexBuffer;

		template<class Archive>
		void serialize(Archive& archive);
	};

	struct VectorKeyframe
	{
		float					seconds;
		Vector3					value = Vector3::Zero;

		template<class Archive>
		void serialize(Archive& archive);
	};

	struct QuaternionKeyframe
	{
		float					seconds;
		Quaternion				value = Quaternion::Identity;

		template<class Archive>
		void serialize(Archive& archive);
	};

	struct NodeAnim
	{
		std::vector<VectorKeyframe>		positionKeyframes;
		std::vector<QuaternionKeyframe>	rotationKeyframes;
		std::vector<VectorKeyframe>		scaleKeyframes;

		template<class Archive>
		void serialize(Archive& archive);
	};

	struct Animation
	{
		std::string					name;
		float						secondsLength;
		std::vector<NodeAnim>		nodeAnims;

		template<class Archive>
		void serialize(Archive& archive);
	};

	struct NodePose
	{
		Vector3    position = Vector3::Zero;
		Quaternion rotation = Quaternion::Identity;
		Vector3    scale = Vector3::One;

		NodePose Lerp(const NodePose& other, float t) const
		{
			NodePose result;
			result.position = Vector3::Lerp(position, other.position, t);
			result.rotation = Quaternion::Slerp(rotation, other.rotation, t);
			result.scale = Vector3::Lerp(scale, other.scale, t);
			return result;
		}
	};

	// アニメーション追加読み込み
	void AppendAnimations(const char* filename);

	// マテリアルデータ取得
	const std::vector<Material>& GetMaterials() const { return materials; }
	std::vector<Material>& GetMaterials() { return materials; }

	// メッシュデータ取得
	const std::vector<Mesh>& GetMeshes() const { return meshes; }
	std::vector<Mesh>& GetMeshes() { return meshes; }

	// アニメーションデータ取得
	const std::vector<Animation>& GetAnimations() const { return animations; }
	std::vector<Animation>& GetAnimations() { return animations; }

	// アニメーションインデックス取得
	int GetAnimationIndex(const char* name) const;

	// ノードデータ取得
	const std::vector<Node>& GetNodes() const { return nodes; }
	std::vector<Node>& GetNodes() { return nodes; }

	// ルートノード取得
	Node* GetRootNode() { return nodes.data(); }

	// ノードインデックス取得
	int GetNodeIndex(const char* name) const;

	// トランスフォーム更新処理
	void UpdateTransform(const Matrix& worldTransform);

	// アニメーション計算
	void ComputeAnimation(int animationIndex, int nodeIndex, float time, NodePose& nodePose) const;
	void ComputeAnimation(int animationIndex, float time, std::vector<NodePose>& nodePoses) const;

	// ノードポーズ設定
	void SetNodePoses(const std::vector<NodePose>& nodePoses);

	// ノードポーズ取得
	void GetNodePoses(std::vector<NodePose>& nodePoses) const;

private:
	// シリアライズ
	void Serialize(const char* filename, uint16_t lastWrite);

	// デシリアライズ
	void Deserialize(const char* filename, uint16_t& lastWrite);

private:

	std::vector<Material>	materials;
	std::vector<Mesh>		meshes;
	std::vector<Node>		nodes;
	std::vector<Animation>	animations;
};
