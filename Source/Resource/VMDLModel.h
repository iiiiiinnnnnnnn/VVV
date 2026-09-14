#pragma once
#include <d3d11.h>
#include <wrl.h>

#include <filesystem>
#include <string>
#include <vector>

#include "Core/Foundation/Common.h"
#include "Core/Foundation/DirectXSerialization.h"
#include "Rendering/Core/RenderContext.h"

// Cereal
#include <cereal/cereal.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/utility.hpp>

class MeshCache;

#include <DirectXTex.h>
#include <DDSTextureLoader.h>

class VMDLModel
{
  public:
	// ノードに追従する物理剛体の設定
	struct VmdlRigidBody
	{
		std::string name = "RIGIDBODY";
		int nodeIndex = -1;
		Vector3 offsetPosition = Vector3::Zero;
		Vector3 offsetRotation = Vector3::Zero;
		float mass = 1.0f;
		bool kinematic = false;

		template <class Archive> void serialize(Archive& archive);
	};

	// ノードに追従する当たり判定の設定
	struct VmdlCollider
	{
		std::string name = "COLLIDER";
		int layer = -1;
		int nodeIndex = -1;
		int shape = 0;
		Vector3 center = Vector3::Zero;
		Vector3 rotation = Vector3::Zero;
		Vector3 size = Vector3::One;
		bool trigger = false;

		template <class Archive> void serialize(Archive& archive);
	};

	// 揺れ物に使うスプリングの設定
	struct VmdlSpring
	{
		std::string name = "SPRING";
		int nodeIndex = -1;
		Vector3 offsetPosition = Vector3::Zero;
		Vector3 offsetRotation = Vector3::Zero;
		float stiffness = 0.5f;
		float drag = 0.2f;

		template <class Archive> void serialize(Archive& archive);
	};

	// スプリング専用の当たり判定設定
	struct VmdlSpringCollider
	{
		std::string name = "SPRING COLLIDER";
		int nodeIndex = -1;
		Vector3 offsetPosition = Vector3::Zero;
		float radius = 0.1f;

		template <class Archive> void serialize(Archive& archive);
	};

	// メッシュ表示を切り替えるモーフ設定
	struct VmdlMorph
	{
		std::string name = "Morph";
		std::vector<uint8_t> meshVisibility;
		bool applyOnInitialize = false;

		template <class Archive> void serialize(Archive& archive);
	};

	// VMDLへ追加する付加設定のまとめ
	struct VmdlExtensionData
	{
		Vector3 rootOffset = Vector3::Zero;
		std::vector<VmdlRigidBody> rigidBodies;
		std::vector<VmdlCollider> colliders;
		std::vector<VmdlSpring> springs;
		std::vector<VmdlSpringCollider> springColliders;
		std::vector<VmdlMorph> morphs;

		template <class Archive> void serialize(Archive& archive);
	};

	// IKで制御する脚のノード設定
	struct VmdlIKLeg
	{
		std::string name = "Leg";
		std::string root;
		std::string mid;
		std::string tip;
		std::string contact;

		template <class Archive> void serialize(Archive& archive);
	};

	// VMDL全体のIK設定
	struct VmdlIKSettings
	{
		static constexpr size_t MaxLegCount = 8;

		int type = 0;
		std::string centerNode = "pelvis";
		std::vector<VmdlIKLeg> legs;

		template <class Archive> void serialize(Archive& archive);
	};

	// IKの曲がる方向を決めるポール設定
	struct VmdlIKPole
	{
		bool custom = false;
		Vector3 position = Vector3::Zero;

		template <class Archive> void serialize(Archive& archive);
	};

	// 接地判定に使うIKレイ設定
	struct VmdlIKRaySettings
	{
		bool custom = false;
		Vector3 startOffset = Vector3(0.0f, 0.2f, 0.0f);
		float length = 0.7f;

		template <class Archive> void serialize(Archive& archive);
	};

	// アニメーションごとのFootIKウェイト
	struct VmdlFootWeightTrack
	{
		static constexpr float DefaultSampleRate = 120.0f;

		std::string animationName;
		float sampleRate = DefaultSampleRate;
		std::vector<float> weights;

		template <class Archive> void serialize(Archive& archive);
	};

	// アニメーション編集用データのまとめ
	struct VmdlAnimationEditorData
	{
		std::vector<VmdlFootWeightTrack> footWeightTracks;

		template <class Archive> void serialize(Archive& archive);
	};

	// ノードに追従するトレイル設定
	struct VmdlTrail
	{
		std::string name = "TRAIL";
		int nodeIndex = -1;
		Vector3 rootOffset = Vector3::Zero;
		Vector3 tipOffset = {-1.0f, 0.0f, 0.0f};
		Color color = {1.0f, 0.9f, 0.3f, 1.0f};
		float tipRatio = 1.0f;
		float lifeTime = 0.5f;
		int maxPoints = 40;
		Vector3 offsetAngle = Vector3::Zero;

		template <class Archive> void serialize(Archive& archive);
	};

	// 有効状態を切り替えるキーフレーム
	struct VmdlBoolKeyframe
	{
		float seconds = 0.0f;
		bool value = true;

		template <class Archive> void serialize(Archive& archive);
	};

	// コライダーの有効状態を制御するトラック
	struct VmdlColliderAnimationTrack
	{
		std::string animationName;
		int colliderIndex = -1;
		std::vector<VmdlBoolKeyframe> keys;

		template <class Archive> void serialize(Archive& archive);
	};

	// モーフを切り替えるキーフレーム
	struct VmdlMorphKeyframe
	{
		float seconds = 0.0f;
		int morphIndex = -1;

		template <class Archive> void serialize(Archive& archive);
	};

	// モーフを制御するアニメーショントラック
	struct VmdlMorphAnimationTrack
	{
		std::string animationName;
		std::vector<VmdlMorphKeyframe> keys;

		template <class Archive> void serialize(Archive& archive);
	};

	// トレイルの有効状態を制御するトラック
	struct VmdlTrailAnimationTrack
	{
		std::string animationName;
		int trailIndex = -1;
		std::vector<VmdlBoolKeyframe> keys;

		template <class Archive> void serialize(Archive& archive);
	};

	// トレイルと再生トラックのまとめ
	struct VmdlTrailData
	{
		std::vector<VmdlTrail> trails;
		std::vector<uint8_t> initialActive;
		std::vector<VmdlTrailAnimationTrack> tracks;

		template <class Archive> void serialize(Archive& archive);
	};

	// アニメーション連動設定のまとめ
	struct VmdlAnimationControlData
	{
		std::vector<uint8_t> colliderInitialActive;
		std::vector<VmdlColliderAnimationTrack> colliderTracks;
		std::vector<VmdlMorphAnimationTrack> morphTracks;

		template <class Archive> void serialize(Archive& archive);
	};

	// VMDL内にはメッシュの位置だけを残し、頂点実体を外部.vmshへ分離する
	struct ExternalMeshGroup
	{
		std::string path;
		std::vector<int> meshIndices;
		std::vector<uint8_t> initialVisibility;
		// meshIndices の各要素が VMSH 内の何番目に対応するか
		// 一部だけVMDLへ戻した後も、残ったメッシュを正しく遅延読み込みするために使う
		std::vector<int> cacheMeshIndices;
		// GLB再取込時にメッシュ番号を復元するための安定キー
		std::vector<std::string> meshKeys;

		template <class Archive> void serialize(Archive& archive);
	};

	// ノードに追従する軽量パーティクル設定
	struct VmdlParticleEmitter
	{
		// 0: Sprite, 1: Ribbon（拡張値はmodel.vfxdataへ保存）
		int rendererType = 0;
		int parentEmitterIndex = -1;
		std::string name = "PARTICLE";
		int nodeIndex = -1;
		std::string texturePath = "Resources/Image/particle256x256.png";
		int columns = 4;
		int rows = 4;
		int frame = 0;
		bool animated = false;
		float animationSpeed = 24.0f;
		int capacity = 256;
		Vector3 offset = Vector3::Zero;
		Vector3 spawnExtents = Vector3::Zero;
		Vector3 velocityMin = Vector3(-0.2f, 0.2f, -0.2f);
		Vector3 velocityMax = Vector3(0.2f, 0.8f, 0.2f);
		Vector3 acceleration = Vector3::Zero;
		float emissionRate = 12.0f;
		int burstCount = 0;
		float lifetimeMin = 0.25f;
		float lifetimeMax = 0.5f;
		Vector2 sizeMin = Vector2(0.1f, 0.1f);
		Vector2 sizeMax = Vector2(0.25f, 0.25f);
		Color color = Color(0.35f, 0.9f, 1.0f, 1.0f);
		float fadeInDuration = 0.0f;
		float fadeOutDuration = 0.15f;
		bool localVelocity = true;
		bool additive = true;
		Vector3 ribbonRootOffset = Vector3::Zero;
		Vector3 ribbonTipOffset = Vector3(-1.0f, 0.0f, 0.0f);
		float ribbonLifetime = 0.18f;
		int ribbonMaxPoints = 40;
		float ribbonTipRatio = 1.0f;
		float ribbonSampleInterval = 0.01f;
		Color ribbonEndColor = Color(0.1f, 0.4f, 1.0f, 0.0f);

		template <class Archive> void serialize(Archive& archive);
	};

	// パーティクルの有効状態を制御するトラック
	struct VmdlParticleAnimationTrack
	{
		std::string animationName;
		int emitterIndex = -1;
		std::vector<VmdlBoolKeyframe> keys;

		template <class Archive> void serialize(Archive& archive);
	};

	// パーティクルと再生トラックのまとめ
	struct VmdlParticleData
	{
		std::vector<VmdlParticleEmitter> emitters;
		std::vector<uint8_t> initialActive;
		std::vector<VmdlParticleAnimationTrack> tracks;

		template <class Archive> void serialize(Archive& archive);
	};

	// 多脚モデル向けのIK補正設定
	struct VmdlMultiLegIKSettings
	{
		float bodyHeightOffset = 0.0f;
		float contactOffset = 0.295f;
		float maxUpCorrection = 2.0f;
		float maxDownCorrection = 5.0f;

		template <class Archive> void serialize(Archive& archive);
	};

	// ノードに配置する空間サウンド設定
	struct VmdlSoundSource
	{
		std::string name = "SOUND SOURCE";
		int nodeIndex = -1;
		int track = 0;
		int variant = -1;
		float pitchMin = 1.0f;
		float pitchMax = 1.0f;
		bool spatial = true;
		float volume = 1.0f;
		float minDistance = 1.0f;
		float maxDistance = 20.0f;
		float lowPassHz = 6500.0f;
		float farLowPassHz = 2200.0f;
		float reverbMix = 0.12f;

		template <class Archive> void serialize(Archive& archive);
	};

	// サウンドトラックの再生バリエーション設定
	struct VmdlSoundSourceBinding
	{
		int track = 0;
		int variant = -1;
		float pitchMin = 1.0f;
		float pitchMax = 1.0f;

		template <class Archive> void serialize(Archive& archive);
	};

	// サウンドを再生するキーフレーム
	struct VmdlSoundKeyframe
	{
		float seconds = 0.0f;
		int sourceIndex = -1;
		int track = 0;
		int variant = -1;
		float volume = 1.0f;
		float pitchMin = 1.0f;
		float pitchMax = 1.0f;

		template <class Archive> void serialize(Archive& archive);
	};

	// サウンド再生を制御するアニメーショントラック
	struct VmdlSoundAnimationTrack
	{
		std::string animationName;
		std::vector<VmdlSoundKeyframe> keys;

		template <class Archive> void serialize(Archive& archive);
	};

	// サウンドソースと再生トラックのまとめ
	struct VmdlSoundData
	{
		std::vector<VmdlSoundSource> sources;
		std::vector<VmdlSoundAnimationTrack> tracks;

		template <class Archive> void serialize(Archive& archive);
	};

	// 範囲内のカメラへ適用する揺れ設定
	struct VmdlCameraShake
	{
		std::string name = "CAMERA SHAKE";
		int nodeIndex = -1;
		float range = 25.0f;
		bool distanceAttenuation = true;
		float duration = 2.0f;
		float intensity = 0.1f;
	};

	// 範囲内のカメラへ適用するラジアルブラー設定
	struct VmdlRadialBlur
	{
		std::string name = "RADIAL BLUR";
		int nodeIndex = -1;
		float range = 25.0f;
		bool distanceAttenuation = true;
		float duration = 5.0f;
		float power = 3.0f;
		float attackRate = 0.15f;
	};

	// カメラ演出を再生するキーフレーム
	struct VmdlPresentationKeyframe
	{
		float seconds = 0.0f;
		int componentIndex = -1;
	};

	// カメラ演出を制御するアニメーショントラック
	struct VmdlPresentationAnimationTrack
	{
		std::string animationName;
		std::vector<VmdlPresentationKeyframe> keys;
	};

	// カメラ演出と再生トラックのまとめ
	struct VmdlPresentationData
	{
		std::vector<VmdlCameraShake> cameraShakes;
		std::vector<VmdlRadialBlur> radialBlurs;
		std::vector<VmdlPresentationAnimationTrack> cameraShakeTracks;
		std::vector<VmdlPresentationAnimationTrack> radialBlurTracks;
	};

	VMDLModel(const char* filename, float sampleRate = 60, const char* savePath = nullptr);
	VMDLModel(const VMDLModel& other);
	VMDLModel(VMDLModel&& other) noexcept;
	VMDLModel& operator=(const VMDLModel& other);
	VMDLModel& operator=(VMDLModel&& other) noexcept;

	std::shared_ptr<VMDLModel> Clone() const;
	// Lightweight snapshot used by transient visual effects. CPU mesh and
	// animation data are omitted while GPU buffers and the current pose are kept.
	std::shared_ptr<VMDLModel> CloneRenderPose() const;
	bool HasSkeleton() const;

	struct Node
	{
		std::string name;
		int parentIndex = -1;
		Vector3 position = Vector3::Zero;
		Quaternion rotation = Quaternion::Identity;
		Vector3 scale = Vector3::One;

		Matrix localTransform;
		Matrix globalTransform;
		Matrix worldTransform;

		Node* parent = nullptr;
		std::vector<Node*> children;

		template <class Archive> void serialize(Archive& archive);
	};

	enum class AlphaMode
	{
		Opaque,
		Mask,
		Blend
	};

	struct Material
	{
		std::string name;
		std::string baseTextureFileName;
		std::string normalTextureFileName;
		std::string emissiveTextureFileName;
		std::string occlusionTextureFileName;
		std::string metalnessRoughnessTextureFileName;

		std::vector<uint8_t> baseTextureDDS;
		std::vector<uint8_t> normalTextureDDS;
		std::vector<uint8_t> emissiveTextureDDS;
		std::vector<uint8_t> occlusionTextureDDS;
		std::vector<uint8_t> metalnessRoughnessTextureDDS;

		Color baseColor = {1, 1, 1, 1};
		Color emissiveColor = {0, 0, 0, 1};
		float metalness = 0.0f;
		float roughness = 0.0f;
		float occlusion = 1.0f;
		float occlusionStrength = 0.0f;
		float shadowStrength = 1.0f;
		float alphaCutoff = 0.5f;
		AlphaMode alphaMode = AlphaMode::Opaque;

		Color fresnelColor = {1, 1, 1, 0};
		float fresnelPower = 0.0f;
		float fresnelStrength = 0.0f;
		int isFlatShading = false;

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> baseMap;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> normalMap;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> emissiveMap;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> occlusionMap;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> metalnessRoughnessMap;

		// GPU転送後にDDSを解放しても、実テクスチャの有無は保持する
		bool hasBaseTexture = false;
		bool hasNormalTexture = false;
		bool hasEmissiveTexture = false;
		bool hasOcclusionTexture = false;
		bool hasMetalnessRoughnessTexture = false;

		template <class Archive> void serialize(Archive& archive);
	};

	struct MaterialPbrSettings
	{
		float occlusion = 1.0f;
		float shadowStrength = 1.0f;

		template <class Archive> void serialize(Archive& archive);
	};

	struct MaterialVMatSettings
	{
		Color fresnelColor = {1, 1, 1, 0};
		float fresnelPower = 0.0f;
		float fresnelStrength = 0.0f;
		int isFlatShading = false;

		template <class Archive> void serialize(Archive& archive);
	};

	// VMDL側で保持するマテリアル調整値
	struct VmdlMaterialData
	{
		std::string name;
		Color baseColor = {1, 1, 1, 1};
		Color emissiveColor = {0, 0, 0, 1};
		float metalness = 0.0f;
		float roughness = 0.0f;
		float occlusion = 1.0f;
		float occlusionStrength = 0.0f;
		float shadowStrength = 1.0f;
		float alphaCutoff = 0.5f;
		AlphaMode alphaMode = AlphaMode::Opaque;
		Color fresnelColor = {1, 1, 1, 0};
		float fresnelPower = 0.0f;
		float fresnelStrength = 0.0f;
		int isFlatShading = false;

		template <class Archive> void serialize(Archive& archive);
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
		Vector3 position = Vector3::Zero;
		Vector3 normal = Vector3::Zero;
		Vector4 tangent = {0, 0, 0, 1};
		Vector2 texcoord = {0, 0};
		Vector4 boneWeight = {1, 0, 0, 0};
		DirectX::XMUINT4 boneIndex = {0, 0, 0, 0};

		template <class Archive> void serialize(Archive& archive);
	};

	struct Bone
	{
		int nodeIndex;
		Matrix offsetTransform;
		Node* node = nullptr;

		template <class Archive> void serialize(Archive& archive);
	};

	struct Mesh
	{
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
		std::vector<Bone> bones;
		int nodeIndex = 0;
		int materialIndex = 0;

		Material* material = nullptr;
		Node* node = nullptr;
		bool isDraw = true;
		uint32_t indexCount = 0;
		Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;

		template <class Archive> void serialize(Archive& archive);
	};

	struct VectorKeyframe
	{
		float seconds;
		Vector3 value = Vector3::Zero;

		template <class Archive> void serialize(Archive& archive);
	};

	struct QuaternionKeyframe
	{
		float seconds;
		Quaternion value = Quaternion::Identity;

		template <class Archive> void serialize(Archive& archive);
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

		template <class Archive> void serialize(Archive& archive);
	};

	struct NodeAnim
	{
		std::vector<VectorKeyframe> positionKeyframes;
		std::vector<QuaternionKeyframe> rotationKeyframes;
		std::vector<VectorKeyframe> scaleKeyframes;

		template <class Archive> void serialize(Archive& archive);
	};

	struct Animation
	{
		std::string name;
		float secondsLength;
		std::vector<NodeAnim> nodeAnims;
		std::vector<FootIKRange> footIKRanges;

		template <class Archive> void serialize(Archive& archive);
	};

	struct NodePose
	{
		Vector3 position = Vector3::Zero;
		Quaternion rotation = Quaternion::Identity;
		Vector3 scale = Vector3::One;

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
		size_t materialIndex, MaterialTextureSlot slot, const std::filesystem::path& texturePath);

	bool ExportMaterialTexture(
		size_t materialIndex, MaterialTextureSlot slot, const std::filesystem::path& savePath);

	bool ClearMaterialTexture(size_t materialIndex, MaterialTextureSlot slot);
	bool ResetMaterialToGLB(size_t materialIndex);

	const std::vector<Mesh>& GetMeshes() const { return meshes; }
	std::vector<Mesh>& GetMeshes() { return meshes; }

	// 指定メッシュを削除し、モーフとマテリアルの参照番号を詰め直す
	bool RemoveMeshes(const std::vector<int>& meshIndices);
	bool ExternalizeMeshes(const std::string& path, const std::vector<int>& meshIndices,
		int activationMorphIndex);
	bool RestoreExternalMeshes(int meshIndex, const std::filesystem::path& vmshPath,
		std::string* error = nullptr);
	bool SetExternalMeshPath(int meshIndex, const std::string& path);
	bool IsExternalMesh(int meshIndex) const;
	const ExternalMeshGroup* GetExternalMeshGroupForMesh(int meshIndex) const;
	const std::vector<ExternalMeshGroup>& GetExternalMeshGroups() const
	{
		return externalMeshGroups;
	}

	const std::vector<Animation>& GetAnimations() const { return animations; }
	std::vector<Animation>& GetAnimations() { return animations; }

	int GetAnimationIndex(const char* name) const;

	const std::vector<Node>& GetNodes() const { return nodes; }
	std::vector<Node>& GetNodes() { return nodes; }

	Node* GetRootNode() { return nodes.data(); }

	int GetNodeIndex(const char* name) const;

	void UpdateTransform(const Matrix& worldTransform);

	const Matrix& GetWorldTransform() const;
	Matrix GetRenderScaleTransform() const;
	Matrix GetScaledAttachmentTransform(const Matrix& unscaledWorldTransform) const;
	Vector3 GetScaledAttachmentVector(const Vector3& unscaledValue) const;
	Vector3 GetUnscaledAttachmentVector(const Vector3& scaledValue) const;
	float GetModelScale() const { return modelScale; }
	void SetModelScale(float value);

	void ComputeAnimation(int animationIndex, int nodeIndex, float time, NodePose& nodePose) const;
	void ComputeAnimation(int animationIndex, float time, std::vector<NodePose>& nodePoses) const;
	float EvaluateFootIKWeight(int animationIndex, float time, int footIndex = -1) const;
	VmdlFootWeightTrack* FindFootWeightTrack(const std::string& animationName, int footIndex);
	const VmdlFootWeightTrack* FindFootWeightTrack(
		const std::string& animationName, int footIndex) const;
	VmdlFootWeightTrack& GetOrCreateFootWeightTrack(
		const std::string& animationName, int footIndex);
	bool GetColliderInitialActive(int colliderIndex) const;
	void SetColliderInitialActive(int colliderIndex, bool active);
	bool EvaluateColliderActive(int animationIndex, float time, int colliderIndex) const;
	VmdlColliderAnimationTrack& GetOrCreateColliderAnimationTrack(
		const std::string& animationName, int colliderIndex);
	bool GetTrailInitialActive(int trailIndex) const;
	void SetTrailInitialActive(int trailIndex, bool active);
	bool EvaluateTrailActive(int animationIndex, float time, int trailIndex) const;
	VmdlTrailAnimationTrack& GetOrCreateTrailAnimationTrack(
		const std::string& animationName, int trailIndex);
	bool GetParticleInitialActive(int emitterIndex) const;
	void SetParticleInitialActive(int emitterIndex, bool active);
	bool EvaluateParticleActive(int animationIndex, float time, int emitterIndex) const;
	VmdlParticleAnimationTrack& GetOrCreateParticleAnimationTrack(
		const std::string& animationName, int emitterIndex);
	VmdlMorphAnimationTrack& GetOrCreateMorphAnimationTrack(const std::string& animationName);
	const VmdlMorphAnimationTrack* FindMorphAnimationTrack(const std::string& animationName) const;
	void ApplyMorphAnimation(int animationIndex, float time);
	void RestoreMorphVisibility(const std::vector<uint8_t>& visibility);
	void RestoreRuntimeMorphVisibility();
	void ApplyInitialMorphs();
	bool ApplyMorph(int morphIndex);
	bool ApplyMorph(const char* name);
	int GetMorphIndex(const char* name) const;
	void NormalizeMorphNames();
	bool SaveVmdl();
	bool SaveVmdl(const std::filesystem::path& filepath);

	// GLB部分のみ交換
	bool ReplaceGLBCache(const std::filesystem::path& filepath, float sampleRate = 60.0f,
		std::string* error = nullptr);
	VmdlExtensionData& GetVmdlExtensionData() { return vmdlExtensionData; }
	const VmdlExtensionData& GetVmdlExtensionData() const { return vmdlExtensionData; }
	VmdlIKSettings& GetVmdlIKSettings() { return vmdlIKSettings; }
	const VmdlIKSettings& GetVmdlIKSettings() const { return vmdlIKSettings; }
	std::vector<VmdlIKPole>& GetVmdlIKPoles() { return vmdlIKPoles; }
	const std::vector<VmdlIKPole>& GetVmdlIKPoles() const { return vmdlIKPoles; }
	std::vector<VmdlIKRaySettings>& GetVmdlIKRaySettings() { return vmdlIKRaySettings; }
	const std::vector<VmdlIKRaySettings>& GetVmdlIKRaySettings() const { return vmdlIKRaySettings; }
	VmdlMultiLegIKSettings& GetVmdlMultiLegIKSettings() { return vmdlMultiLegIKSettings; }
	const VmdlMultiLegIKSettings& GetVmdlMultiLegIKSettings() const
	{
		return vmdlMultiLegIKSettings;
	}
	void ResetVmdlIKLegsForType();
	bool AutoAssignVmdlIKNodes();
	VmdlAnimationEditorData& GetVmdlAnimationEditorData() { return vmdlAnimationEditorData; }
	const VmdlAnimationEditorData& GetVmdlAnimationEditorData() const
	{
		return vmdlAnimationEditorData;
	}
	VmdlAnimationControlData& GetVmdlAnimationControlData() { return vmdlAnimationControlData; }
	const VmdlAnimationControlData& GetVmdlAnimationControlData() const
	{
		return vmdlAnimationControlData;
	}
	VmdlTrailData& GetVmdlTrailData() { return vmdlTrailData; }
	const VmdlTrailData& GetVmdlTrailData() const { return vmdlTrailData; }
	VmdlParticleData& GetVmdlParticleData() { return vmdlParticleData; }
	const VmdlParticleData& GetVmdlParticleData() const { return vmdlParticleData; }
	VmdlSoundData& GetVmdlSoundData() { return vmdlSoundData; }
	const VmdlSoundData& GetVmdlSoundData() const { return vmdlSoundData; }
	VmdlPresentationData& GetVmdlPresentationData() { return vmdlPresentationData; }
	const VmdlPresentationData& GetVmdlPresentationData() const { return vmdlPresentationData; }
	void SetNodePoses(const std::vector<NodePose>& nodePoses);

	void GetNodePoses(std::vector<NodePose>& nodePoses) const;

  private:
	struct RenderPoseCloneTag {};
	VMDLModel(const VMDLModel& other, RenderPoseCloneTag);

	friend class MeshCache;

	static std::string MakeFootWeightTrackKey(const std::string& animationName, int footIndex);
	void NormalizeAttachmentNames();
	void NormalizeVmdlIKRaySettings();
	void ApplyForwardDirectionCorrection();

	void Serialize(const char* filename);
	void Deserialize(const char* filename);

	// 分割形式
	static constexpr uint32_t VmdlCompressionVersion = 9;

	static void BuildEmbeddedDDSFromFileOrSRV(ID3D11Device* device, const std::filesystem::path& dirpath,
		const std::string& textureFileName, ID3D11ShaderResourceView* srv,
		std::vector<uint8_t>& outDDS);
	void BuildMaterialEmbeddedDDS(
		ID3D11Device* device, const std::filesystem::path& dirpath, Material& material);
	void CreateSRVFromEmbeddedDDSOrFile(ID3D11Device* device, const std::filesystem::path& dirpath,
		const std::string& textureFileName, const std::vector<uint8_t>& embeddedDDS,
		uint32_t dummyColor, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv);
	void BuildMaterialTextureResources(
		ID3D11Device* device, const std::filesystem::path& dirpath, Material& material);
	void SyncMaterialTextureToSource(size_t materialIndex, MaterialTextureSlot slot);
	std::vector<VmdlMaterialData> CaptureVmdlMaterialData() const;
	void ApplyVmdlMaterialData(const std::vector<VmdlMaterialData>& data);
	bool ApplyMorphToMeshes(int morphIndex);
	void CaptureRuntimeMorphVisibility();

	void RebuildRuntimeReferences();

	std::vector<Material> materials;
	std::vector<Material> sourceMaterials;
	std::vector<Mesh> meshes;
	std::vector<Node> nodes;
	std::vector<Animation> animations;
	VmdlExtensionData vmdlExtensionData;
	VmdlIKSettings vmdlIKSettings;
	std::vector<VmdlIKPole> vmdlIKPoles;
	std::vector<VmdlIKRaySettings> vmdlIKRaySettings;
	VmdlMultiLegIKSettings vmdlMultiLegIKSettings;
	VmdlAnimationEditorData vmdlAnimationEditorData;
	VmdlAnimationControlData vmdlAnimationControlData;
	VmdlTrailData vmdlTrailData;
	VmdlParticleData vmdlParticleData;
	VmdlSoundData vmdlSoundData;
	VmdlPresentationData vmdlPresentationData;
	std::vector<uint8_t> runtimeMorphVisibility;
	std::vector<ExternalMeshGroup> externalMeshGroups;
	float modelScale = 1.0f;
	Matrix worldTransform = Matrix::Identity;

	std::filesystem::path modelCacheFilepath;
};
