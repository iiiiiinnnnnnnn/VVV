// VmdlEditorScene.h

#pragma once

#include "Gameplay/Lighting/LightManager.h"
#include "Gameplay/Scene/Scene.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class Camera;
class VMDLModel;
class Object;
class RenderTarget;

class VmdlEditorScene : public Scene
{
public:
	VmdlEditorScene(SceneMessage message = nullptr);
	~VmdlEditorScene() override;

protected:
	void OnDrawGUI() override;
	bool OnRequestExit() override;

private:
	void RenderPreview();
	void DrawMenuBar();
	void DrawHierarchy();
	void DrawNodeTree(int nodeIndex);
	void DrawViewport();
	void DrawProperty();
	void DrawTimeline();
	void DrawIkSettings();
	void DrawShapeEditor();
	void DrawMaterialEditor();
	void DrawAnimationEventEditor();
	void DrawAttachedData(int nodeIndex);
	void DrawNodeContextMenu(int nodeIndex);
	void DrawAnimationCurves();
	void ApplyAnimationPreview();
	void ResetAnimationControlPreview();
	void RecordSelectedNodeKey();
	void AddAnimationKey();
	void MarkDirty();
	void UpdateModelPlacement();
	void UpdateWindowTitle();
	void OpenVmdl();
	void ImportGlb();
	void SaveVmdl();
	void SaveVmdlAs();
	void LoadModel(const std::filesystem::path& filepath, bool importGlb);
	void ErrorMessage(const std::string& message);

	std::shared_ptr<VMDLModel> model;
	std::filesystem::path documentPath;
	std::filesystem::path displayPath;
	std::wstring previousWindowTitle;
	std::unique_ptr<RenderTarget> previewTarget;
	std::unique_ptr<Object> cameraOwner;
	Camera* editorCamera = nullptr;
	LightManager editorLights;
	int selectedNode = -1;
	int selectedMesh = -1;
	int selectedAnimation = -1;
	float animationTime = 0.0f;
	float playbackSpeed = 1.0f;
	float bottomPanelHeight = 430.0f;
	float cameraYaw = 0.55f;
	float cameraPitch = 0.35f;
	float cameraDistance = 5.0f;
	Vector3 cameraFocusOffset = Vector3::Zero;
	int gizmoOperation = 120;
	int selectedShape = -1;
	int selectedMaterial = -1;
	int selectedColliderEventTarget = 0;
	int selectedTrailEventTarget = 0;
	int selectedShapeEventTarget = 0;
	bool usePbr = true;
	Color solidColor = Color(0.72f, 0.72f, 0.75f, 1.0f);
	LONG_PTR previousWindowStyle = 0;
	WINDOWPLACEMENT previousWindowPlacement{sizeof(WINDOWPLACEMENT)};
	bool restoreWindowOnExit = false;
	Vector3 rootOffset = Vector3::Zero;
	float modelScale = 1.0f;
	bool dirty = false;
	bool animationPlaying = false;
	bool animationLoop = true;
	bool animationRecording = false;
	bool draggingAnimationKey = false;
	bool scrubbingAnimationTime = false;
	bool paintingFootWeight = false;
	int paintingFootWeightIndex = -1;
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
	bool showMesh = true;
	bool showFaces = true;
	bool showRigidBody = true;
	bool showCollider = true;
	bool showSpring = true;
	bool showSpringCollider = true;
	bool showTrail = true;
	bool showBones = true;
	bool showGrid = true;
	bool showExportWarning = false;
	bool exiting = false;
	std::vector<uint8_t> previewColliderActive;
	std::vector<uint8_t> previewTrailActive;
};
