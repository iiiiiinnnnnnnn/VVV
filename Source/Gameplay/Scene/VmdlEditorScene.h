#pragma once

#include "Audio/SoundTrackRegistry.h"
#include "Gameplay/Lighting/LightManager.h"
#include "Gameplay/Scene/Scene.h"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class Camera;
class VMDLModel;
class Object;
class Actor;
class Animator;
class Rigidbody;
class MeshCollider;
class RenderTarget;
class TrailRenderComponent;
class VMDLParticleEmitterComponent;
class SpringBone;
class MeshCache;

class VmdlEditorScene : public Scene
{
  public:
	VmdlEditorScene();
	VmdlEditorScene(std::filesystem::path filepath);
	~VmdlEditorScene() override;

  protected:
	void OnUpdate() override;
	void ToggleDebugDisplay() override;
	void OnDrawGUI() override;
	bool OnRequestExit() override;

  private:
	enum class AttachedComponentType
	{
		None,
		RigidBody,
		Collider,
		Spring,
		SpringCollider,
		Trail,
		Particle,
		SoundSource,
		CameraShake,
		RadialBlur,
	};

	// 画面描画
	void RenderPreview();
	void DrawMenuBar();
	void DrawHierarchy();
	void DrawNodeTree(int nodeIndex);
	void DrawHierarchyComponent(int nodeIndex, AttachedComponentType type, int componentIndex,
		const char* label, const std::string& name);

	// 階層選択と一括操作
	void SelectNode(int nodeIndex, bool toggleSelection);
	bool IsNodeSelected(int nodeIndex) const;
	// メッシュを単独選択またはCtrlによる複数選択へ反映する
	void SelectMesh(int meshIndex, bool toggleSelection);
	// 指定メッシュが複数選択に含まれているかを返す
	bool IsMeshSelected(int meshIndex) const;
	void AddAttachedComponentToSelectedNodes(AttachedComponentType type);
	std::string MakeUniqueAttachedComponentName(
		AttachedComponentType type, const std::string& baseName) const;
	void DrawViewport();
	void DrawProperty();
	void DrawTimeline();
	void DrawIkSettings();
	void DrawMorphEditor();
	void DrawMaterialEditor();
	bool DrawSoundTrackSelector(const char* label, int& track);
	std::string SoundTrackLabel(int track) const;
	void DrawAttachedData(int nodeIndex);
	void DrawNodeContextMenu(int nodeIndex);
	void DrawAnimationCurves();
	void DrawAnimationEventEditor();
	void DrawFootIkPreviewWindow();

	// 階層検索
	bool MatchesHierarchySearch(const std::string& text) const;
	bool ComponentMatchesHierarchySearch(const char* typeNames, const std::string& name) const;
	bool NodeMatchesHierarchySearch(int nodeIndex) const;

	// レイアウト設定
	void LoadLayoutSettings();
	void SaveLayoutSettings();

	// プレビュー制御
	void ApplyAnimationPreview();
	void StartUnifiedPreview();
	void StopUnifiedPreview(bool rewind);
	void StartPresentationPreview();
	void PlayPresentationPreviewEvents(int animationIndex, float beginTime, float endTime);
	void PlayAnimationSoundPreview(int animationIndex, float beginTime, float endTime);
	bool ApplyFootIkPreview();
	void RebuildFootIkPreview();
	void LoadFootIkTestStage();
	void UpdateFootIkTestStage();
	void ResetAnimationControlPreview();
	void RebuildSpringPreview();
	bool UpdateSpringPreview();
	void RebuildTrailPreview();
	void UpdateTrailPreview(const RenderContext& rc);
	void RebuildParticlePreview();
	void UpdateParticlePreview(const RenderContext& rc);
	void UpdateExternalMeshPreview();
	void ExportParticlePrefab(int emitterIndex);
	void ImportParticlePrefab(int emitterIndex);
	void RecordSelectedNodeKey();
	void MarkDirty();
	void UpdateModelFraming();
	std::string MakeUniqueMorphName(const std::string& baseName) const;
	static std::string MakeNodeLabel(int nodeIndex, const std::string& nodeName);

	// ファイル操作
	void OpenVmdl();
	void ImportGlb();
	void ReplaceGlbCache();
	void AppendAnimationGlb();
	// 選択メッシュをVMSHへ分離する
	void SeparateMeshToCache(int meshIndex);
	void SeparateMorphMeshes(int morphIndex);
	std::filesystem::path MakeMeshCachePath(
		int meshIndex, const std::string& morphName = {}) const;
	void RestoreExternalMesh(int meshIndex);
	bool ResolveExternalMeshPath(int meshIndex, std::filesystem::path& resolvedPath);
	void ResolveMissingExternalMeshes();
	void SaveVmdl();
	void SaveVmdlAs();
	void LoadModel(
		const std::filesystem::path& filepath, const std::filesystem::path& importDestination = {});
	void ErrorMessage(const std::string& message);

	// 編集対象
	std::shared_ptr<VMDLModel> model;
	std::unordered_map<int, std::shared_ptr<MeshCache>> externalMeshPreviewCaches;
	SoundTrackRegistry editorSoundTracks;
	std::filesystem::path documentPath;
	std::filesystem::path recentModelPath;
	std::unique_ptr<RenderTarget> previewSceneTarget;
	std::unique_ptr<RenderTarget> previewTarget;
	std::unique_ptr<Object> cameraOwner;
	Camera* editorCamera = nullptr;
	LightManager editorLights;

	// 選択状態
	int selectedNode = -1;
	std::vector<int> selectedNodes;
	int selectedMesh = -1;
	std::vector<int> selectedMeshes;
	AttachedComponentType selectedComponentType = AttachedComponentType::None;
	int selectedComponentIndex = -1;
	bool focusSelectedComponent = false;

	// 階層検索
	std::string hierarchySearch;
	std::string hierarchySearchUpper;

	// アニメーション選択
	int selectedAnimation = -1;
	float animationTime = 0.0f;
	float playbackSpeed = 1.0f;

	// レイアウト設定
	float bottomPanelHeight = 430.0f;
	float propertyPanelWidth = -1.0f;
	float viewportPanelWidth = -1.0f;
	float loadedBottomPanelRatio = -1.0f;
	float loadedPropertyPanelRatio = -1.0f;
	float loadedViewportPanelRatio = -1.0f;
	float layoutColumnWidth = 0.0f;
	float layoutTotalHeight = 0.0f;
	bool layoutInitialized = false;
	// Win32のサイズ変更が次のImGuiフレームへ反映されるまで初期化を待つ
	bool layoutWindowMetricsPending = true;

	// カメラ状態
	float cameraYaw = 0.55f;
	float cameraPitch = 0.35f;
	float cameraDistance = 5.0f;
	float targetCameraDistance = 5.0f;
	float previewCameraMoveSpeed = 5.0f;
	Vector3 cameraFocusOffset = Vector3::Zero;
	bool cameraReturningToFront = false;
	int gizmoOperation = 120;
	bool viewportRotationDragging = false;
	int selectedMorph = -1;
	int selectedMaterial = -1;
	int selectedColliderEventTarget = 0;
	int selectedTrailEventTarget = 0;
	int selectedParticleEventTarget = 0;
	int selectedMorphEventTarget = 0;
	bool restoreWindowPending = true;
	bool savedWindowPlacementValid = false;
	bool savedWindowMaximized = true;
	int savedWindowX = 100;
	int savedWindowY = 100;
	int savedWindowWidth = 1280;
	int savedWindowHeight = 720;
	Vector3 editorLightDirection = Vector3(-0.7f, -0.6f, 0.0f);

	// 編集状態
	bool dirty = false;
	bool unifiedPreviewActive = false;
	bool animationPlaying = false;
	bool animationSoundPreviewStarting = true;
	bool particlePreviewBurstPending = false;
	float cameraShakePreviewTimer = 0.0f;
	float cameraShakePreviewDuration = 0.0f;
	float cameraShakePreviewIntensity = 0.0f;
	float radialBlurPreviewTimer = 0.0f;
	float radialBlurPreviewDuration = 0.0f;
	float radialBlurPreviewPower = 0.0f;
	float radialBlurPreviewAttackRate = 0.15f;
	bool animationLoop = true;
	bool animationRecording = false;
	bool draggingAnimationKey = false;
	bool scrubbingAnimationTime = false;
	bool paintingFootWeight = false;
	int paintingFootWeightIndex = -1;
	int paintingFootWeightLastSample = -1;
	bool positionTrackExpanded = true;
	bool rotationTrackExpanded = true;
	bool scaleTrackExpanded = true;
	int timelineEventContextKind = -1;
	int timelineEventContextTarget = -1;
	int timelineEventContextKey = -1;
	float timelineEventContextTime = 0.0f;
	int selectedKeyTrack = -1;
	int selectedKeyIndex = -1;
	int draggingNumericTrack = -1;
	int draggingNumericComponent = -1;

	// 表示設定
	enum class PreviewShadingMode
	{
		Pbr,
		Unlit,
		Solid,
	};

	PreviewShadingMode previewShadingMode = PreviewShadingMode::Pbr;
	Color solidColor = Color(0.72f, 0.72f, 0.75f, 1.0f);
	bool showMesh = true;
	bool showDebugOverlays = true;
	bool showRigidBody = true;
	bool showCollider = true;
	bool showSoundRange = true;
	bool showSpring = true;
	bool showSpringCollider = true;
	bool showTrail = true;
	bool showParticle = true;
	bool showBones = true;
	bool showIkPole = true;
	bool showGrid = true;
	bool showSetScaleWindow = false;
	float setScaleValue = 1.0f;
	bool showPhysicsLayerWindow = false;
	bool exiting = false;
	std::vector<uint8_t> previewColliderActive;
	std::vector<uint8_t> previewTrailActive;
	std::vector<uint8_t> previewParticleActive;

	// Springプレビュー
	std::unique_ptr<Object> springPreviewOwner;
	std::vector<SpringBone*> springPreviewComponents;
	std::string springPreviewSignature;
	int springPreviewAnimation = -1;
	float springPreviewAnimationTime = 0.0f;

	// トレイルプレビュー
	std::unique_ptr<Object> trailPreviewOwner;
	std::vector<TrailRenderComponent*> trailPreviewComponents;
	std::string trailPreviewSignature;
	int trailPreviewAnimation = -1;
	float trailPreviewAnimationTime = 0.0f;

	// パーティクルプレビュー
	std::unique_ptr<Object> particlePreviewOwner;
	std::vector<VMDLParticleEmitterComponent*> particlePreviewComponents;
	int particlePreviewAnimation = -1;
	float particlePreviewAnimationTime = 0.0f;

	// Foot IKプレビュー
	std::shared_ptr<VMDLModel> footIkTestStageModel;
	std::unique_ptr<Actor> footIkTestStageActor;
	Rigidbody* footIkTestStageRigidbody = nullptr;
	MeshCollider* footIkTestStageCollider = nullptr;
	std::unique_ptr<Actor> footIkPreviewOwner;
	Animator* footIkPreviewAnimator = nullptr;
	std::string footIkPreviewSignature;
	Vector3 footIkTestStagePosition = Vector3::Zero;
	Vector3 footIkTestStageRotation = Vector3::Zero;
	float footIkTestStageSize = 100.0f;
	bool showFootIkPreviewWindow = false;
	bool showFootIkTestStage = false;
	bool showFootIkDebug = true;
};
