// VMDLModel.h

#pragma once
#include <d3d11.h>
#include <wrl.h>

#include <filesystem>
#include <string>
#include <vector>

#include "Core/Foundation/Common.h"
#include "Core/Foundation/DirectXSerialization.h"
#include "Rendering/Core/ShaderParam.h"
#include "Rendering/Core/RenderContext.h"

// Cereal
#include <cereal/cereal.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/utility.hpp>

#include <DirectXTex.h>
#include <DDSTextureLoader.h>

class VMDLModel
{
public:
	struct VmdlRigidBody
	{
		std::string name = "Rigidbody";
		int nodeIndex = -1;
		Vector3 offsetPosition = Vector3::Zero;
		Vector3 offsetRotation = Vector3::Zero;
		float mass = 1.0f;
		bool kinematic = false;

		template<class Archive>
		void serialize(Archive& archive);
	};

	struct VmdlCollider
	{
		std::string name = "Collider";
		int nodeIndex = -1;
		int shape = 0;
		Vector3 center = Vector3::Zero;
		Vector3 rotation = Vector3::Zero;
		Vector3 size = Vector3::One;
		bool trigger = false;

		template<class Archive>
		void serialize(Archive& archive);
	};

	struct VmdlSpring
	{
		std::string name = "Spring";
		int nodeIndex = -1;
		Vector3 offsetPosition = Vector3::Zero;
		Vector3 offsetRotation = Vector3::Zero;
		float stiffness = 0.5f;
		float drag = 0.2f;

		template<class Archive>
		void serialize(Archive& archive);
	};

	struct VmdlSpringCollider
	{
		std::string name = "Spring Collider";
		int nodeIndex = -1;
		Vector3 offsetPosition = Vector3::Zero;
		float radius = 0.1f;

		template<class Archive>
		void serialize(Archive& archive);
	};

	struct VmdlShape
	{
		std::string name = "Shape";
		std::vector<uint8_t> meshVisibility;

		template<class Archive>
		void serialize(Archive& archive);
	};

	struct VmdlExtensionData
	{
		Vector3 rootOffset = Vector3::Zero;
		std::vector<VmdlRigidBody> rigidBodies;
		std::vector<VmdlCollider> colliders;
		std::vector<VmdlSpring> springs;
		std::vector<VmdlSpringCollider> springColliders;
		std::vector<VmdlShape> shapes;

		template<class Archive>
		void serialize(Archive& archive);
	};

	struct VmdlIKSettings
	{
		int type = 0;
		std::string pelvis = "pelvis";
		std::string leftThigh = "thigh_l";
		std::string leftCalf = "calf_l";
		std::string leftFoot = "foot_l";
		std::string leftBall = "ball_l";
		std::string rightThigh = "thigh_r";
		std::string rightCalf = "calf_r";
		std::string rightFoot = "foot_r";
		std::string rightBall = "ball_r";

		template<class Archive>
		void serialize(Archive& archive);
	};

	struct VmdlFootWeightTrack
	{
		std::string animationName;
		float sampleRate = 60.0f;
		std::vector<float> weights;

		template<class Archive>
		void serialize(Archive& archive);
	};

	struct VmdlAnimationEditorData
	{
		std::vector<VmdlFootWeightTrack> footWeightTracks;

		template<class Archive>
		void serialize(Archive& archive);
	};

	struct VmdlTrail
	{
		std::string name = "Trail";
		int nodeIndex = -1;
		Vector3 rootOffset = Vector3::Zero;
		Vector3 tipOffset = {-1.0f, 0.0f, 0.0f};
		Color color = {1.0f, 0.9f, 0.3f, 1.0f};
		float tipRatio = 1.0f;
		float lifeTime = 0.5f;
		int maxPoints = 40;

		template<class Archive>
		void serialize(Archive& archive);
	};

	struct VmdlBoolKeyframe
	{
		float seconds = 0.0f;
		bool value = true;

		template<class Archive>
		void serialize(Archive& archive);
	};

	struct VmdlColliderAnimationTrack
	{
		std::string animationName;
		int colliderIndex = -1;
		std::vector<VmdlBoolKeyframe> keys;

		template<class Archive>
		void serialize(Archive& archive);
	};

	struct VmdlShapeKeyframe
	{
		float seconds = 0.0f;
		int shapeIndex = -1;

		template<class Archive>
		void serialize(Archive& archive);
	};

	struct VmdlShapeAnimationTrack
	{
		std::string animationName;
		std::vector<VmdlShapeKeyframe> keys;

		template<class Archive>
		void serialize(Archive& archive);
	};

	struct VmdlTrailAnimationTrack
	{
		std::string animationName;
		int trailIndex = -1;
		std::vector<VmdlBoolKeyframe> keys;

		template<class Archive>
		void serialize(Archive& archive);
	};

	struct VmdlTrailData
	{
		std::vector<VmdlTrail> trails;
		std::vector<uint8_t> initialActive;
		std::vector<VmdlTrailAnimationTrack> tracks;

		template<class Archive>
		void serialize(Archive& archive);
	};

	struct VmdlPlacementData
	{
		float scale = 1.0f;
		bool initialized = false;

		template<class Archive>
		void serialize(Archive& archive);
	};

	struct VmdlAnimationControlData
	{
		std::vector<uint8_t> colliderInitialActive;
		std::vector<VmdlColliderAnimationTrack> colliderTracks;
		std::vector<VmdlShapeAnimationTrack> shapeTracks;

		template<class Archive>
		void serialize(Archive& archive);
	};

	VMDLModel(
		const char* filename,
		float sampleRate = 60,
		bool importRawModel = false,
		const char* cacheFilename = nullptr,
		bool saveImportedCache = true);
	VMDLModel(const VMDLModel& other);
	VMDLModel(VMDLModel&& other) noexcept;
	VMDLModel& operator=(const VMDLModel& other);
	VMDLModel& operator=(VMDLModel&& other) noexcept;

	std::shared_ptr<VMDLModel> Clone() const;
	bool HasSkeleton() const;

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

		Node* parent = nullptr;
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
		std::string			name;
		std::string			baseTextureFileName;
		std::string			normalTextureFileName;
		std::string			emissiveTextureFileName;
		std::string			occlusionTextureFileName;
		std::string			metalnessRoughnessTextureFileName;

		std::vector<uint8_t>	baseTextureDDS;
		std::vector<uint8_t>	normalTextureDDS;
		std::vector<uint8_t>	emissiveTextureDDS;
		std::vector<uint8_t>	occlusionTextureDDS;
		std::vector<uint8_t>	metalnessRoughnessTextureDDS;

		Color				baseColor = {1, 1, 1, 1};
		Color				emissiveColor = {1, 1, 1, 1};
		float				metalness = 0.0f;
		float				roughness = 0.0f;
		float				occlusion = 1.0f;
		float				occlusionStrength = 0.0f;
		float				shadowStrength = 1.0f;
		float				alphaCutoff = 0.5f;
		AlphaMode			alphaMode = AlphaMode::Opaque;
		ShaderParamList		shaderParams = {};

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	baseMap;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	normalMap;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	emissiveMap;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	occlusionMap;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	metalnessRoughnessMap;

		template<class Archive>
		void serialize(Archive& archive);
	};

	struct MaterialPbrSettings
	{
		float occlusion = 1.0f;
		float shadowStrength = 1.0f;

		template<class Archive>
		void serialize(Archive& archive);
	};

	enum class MaterialTextureSlot
	{
		BaseColor,
		Normal,
		MetalnessRoughness,
		Occlusion,
		Emissive
	};

	struct Vertex
	{
		Vector3				position = Vector3::Zero;
		Vector3				normal = Vector3::Zero;
		Vector4				tangent = {0, 0, 0, 1};
		Vector2				texcoord = {0, 0};
		Vector4				boneWeight = {1, 0, 0, 0};
		DirectX::XMUINT4	boneIndex = {0, 0, 0, 0};

		template<class Archive>
		void serialize(Archive& archive);
	};

	struct Bone
	{
		int					nodeIndex;
		Matrix				offsetTransform;
		Node* node = nullptr;

		template<class Archive>
		void serialize(Archive& archive);
	};

	struct Mesh
	{
		std::vector<Vertex>		vertices;
		std::vector<uint32_t>	indices;
		std::vector<Bone>		bones;
		int			nodeIndex = 0;
		int			materialIndex = 0;

		Material* material = nullptr;
		Node* node = nullptr;
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

	struct FootIKRange
	{
		std::string name = "FootIK";
		int footIndex = -1;
		float startRatio = 0.0f;
		float endRatio = 1.0f;
		float weight = 1.0f;
		float fadeInRatio = 0.03f;
		float fadeOutRatio = 0.03f;

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
		std::vector<FootIKRange>	footIKRanges;

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

	void AppendAnimations(const char* filename);

	const std::vector<Material>& GetMaterials() const { return materials; }
	std::vector<Material>& GetMaterials() { return materials; }

	bool ReplaceMaterialTexture(
		size_t materialIndex,
		MaterialTextureSlot slot,
		const std::filesystem::path& texturePath);

	bool ExportMaterialTexture(
		size_t materialIndex,
		MaterialTextureSlot slot,
		const std::filesystem::path& savePath);

	bool ClearMaterialTexture(
		size_t materialIndex, MaterialTextureSlot slot);

	const std::vector<Mesh>& GetMeshes() const { return meshes; }
	std::vector<Mesh>& GetMeshes() { return meshes; }

	const std::vector<Animation>& GetAnimations() const { return animations; }
	std::vector<Animation>& GetAnimations() { return animations; }

	int GetAnimationIndex(const char* name) const;

	const std::vector<Node>& GetNodes() const { return nodes; }
	std::vector<Node>& GetNodes() { return nodes; }

	Node* GetRootNode() { return nodes.data(); }

	int GetNodeIndex(const char* name) const;

	void UpdateTransform(const Matrix& worldTransform);

	const Matrix& GetWorldTransform() const;

	void ComputeAnimation(int animationIndex, int nodeIndex, float time, NodePose& nodePose) const;
	void ComputeAnimation(int animationIndex, float time, std::vector<NodePose>& nodePoses) const;
	float EvaluateFootIKWeight(int animationIndex, float time, int footIndex = -1) const;
	VmdlFootWeightTrack* FindFootWeightTrack(const std::string& animationName, int footIndex);
	const VmdlFootWeightTrack* FindFootWeightTrack(const std::string& animationName, int footIndex) const;
	VmdlFootWeightTrack& GetOrCreateFootWeightTrack(const std::string& animationName, int footIndex);
	bool GetColliderInitialActive(int colliderIndex) const;
	void SetColliderInitialActive(int colliderIndex, bool active);
	bool EvaluateColliderActive(int animationIndex, float time, int colliderIndex) const;
	VmdlColliderAnimationTrack& GetOrCreateColliderAnimationTrack(const std::string& animationName, int colliderIndex);
	bool GetTrailInitialActive(int trailIndex) const;
	void SetTrailInitialActive(int trailIndex, bool active);
	bool EvaluateTrailActive(int animationIndex, float time, int trailIndex) const;
	VmdlTrailAnimationTrack& GetOrCreateTrailAnimationTrack(const std::string& animationName, int trailIndex);
	VmdlShapeAnimationTrack& GetOrCreateShapeAnimationTrack(const std::string& animationName);
	const VmdlShapeAnimationTrack* FindShapeAnimationTrack(const std::string& animationName) const;
	void ApplyShapeAnimation(int animationIndex, float time);
	void RestoreShapeVisibility(const std::vector<uint8_t>& visibility);
	void RestoreRuntimeShapeVisibility();
	bool ApplyShape(int shapeIndex);
	bool ApplyShape(const char* name);
	int GetShapeIndex(const char* name) const;
	bool SaveVmdl();
	bool SaveVmdl(const std::filesystem::path& filepath);
	VmdlExtensionData& GetVmdlExtensionData() { return vmdlExtensionData; }
	const VmdlExtensionData& GetVmdlExtensionData() const { return vmdlExtensionData; }
	VmdlIKSettings& GetVmdlIKSettings() { return vmdlIKSettings; }
	const VmdlIKSettings& GetVmdlIKSettings() const { return vmdlIKSettings; }
	VmdlAnimationEditorData& GetVmdlAnimationEditorData() { return vmdlAnimationEditorData; }
	const VmdlAnimationEditorData& GetVmdlAnimationEditorData() const { return vmdlAnimationEditorData; }
	VmdlAnimationControlData& GetVmdlAnimationControlData() { return vmdlAnimationControlData; }
	const VmdlAnimationControlData& GetVmdlAnimationControlData() const { return vmdlAnimationControlData; }
	VmdlTrailData& GetVmdlTrailData() { return vmdlTrailData; }
	const VmdlTrailData& GetVmdlTrailData() const { return vmdlTrailData; }
	VmdlPlacementData& GetVmdlPlacementData() { return vmdlPlacementData; }
	const VmdlPlacementData& GetVmdlPlacementData() const { return vmdlPlacementData; }
	static bool IsCacheUpToDate(
		const std::filesystem::path& sourcePath,
		const std::filesystem::path& cachePath);

	void SetNodePoses(const std::vector<NodePose>& nodePoses);

	void GetNodePoses(std::vector<NodePose>& nodePoses) const;

private:
	static std::string MakeFootWeightTrackKey(const std::string& animationName, int footIndex);

	void Serialize(const char* filename, uint64_t lastWrite);
	void Deserialize(const char* filename, uint64_t& lastWrite);

	static constexpr uint64_t ModelCacheVersion = 4;
	static uint64_t MakeModelCacheStamp(uint64_t sourceLastWrite);
	static bool ReadModelCacheStamp(const std::filesystem::path& filepath, uint64_t& stamp);

	static HRESULT SaveScratchImageToDDSBytes(const DirectX::ScratchImage& sourceImage, std::vector<uint8_t>& outDDS);
	static HRESULT ConvertTextureFileToDDSBytes(const std::filesystem::path& texturePath, std::vector<uint8_t>& outDDS);
	static HRESULT ConvertSRVToDDSBytes(ID3D11Device* device, ID3D11ShaderResourceView* srv, std::vector<uint8_t>& outDDS);
	void BuildEmbeddedDDSFromFileOrSRV(ID3D11Device* device, const std::filesystem::path& dirpath, const std::string& textureFileName, ID3D11ShaderResourceView* srv, std::vector<uint8_t>& outDDS);
	void BuildMaterialEmbeddedDDS(ID3D11Device* device, const std::filesystem::path& dirpath, Material& material);
	void CreateSRVFromEmbeddedDDSOrFile(ID3D11Device* device, const std::filesystem::path& dirpath, const std::string& textureFileName, const std::vector<uint8_t>& embeddedDDS, uint32_t dummyColor, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv);
	void BuildMaterialTextureResources(ID3D11Device* device, const std::filesystem::path& dirpath, Material& material);
	bool ApplyShapeToMeshes(int shapeIndex);
	void CaptureRuntimeShapeVisibility();

	void RebuildRuntimeReferences();

	std::vector<Material> materials;
	std::vector<Mesh> meshes;
	std::vector<Node> nodes;
	std::vector<Animation> animations;
	VmdlExtensionData vmdlExtensionData;
	VmdlIKSettings vmdlIKSettings;
	VmdlAnimationEditorData vmdlAnimationEditorData;
	VmdlAnimationControlData vmdlAnimationControlData;
	VmdlTrailData vmdlTrailData;
	VmdlPlacementData vmdlPlacementData;
	std::vector<uint8_t> runtimeShapeVisibility;

	std::filesystem::path modelCacheFilepath;
	uint64_t modelCacheLastWrite = 0;
};

