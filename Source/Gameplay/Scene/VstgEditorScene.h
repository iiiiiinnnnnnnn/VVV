#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Gameplay/Lighting/LightManager.h"
#include "Gameplay/Scene/Scene.h"
#include "Resource/VSTG.h"
#include "Rendering/Core/VMatRenderParams.h"

class StageLoader;
class Terrain;
class NavMeshActor;
class FreeCameraController;
class Camera;
class Object;
class RenderTarget;
class VMDLModel;

class VstgEditorScene : public Scene
{
  public:
	VstgEditorScene();
	~VstgEditorScene() override;

  protected:
	void OnInit() override;
	void OnUninit() override;
	void OnUpdate() override;
	void ConfigureRenderSettings(RenderSettings& settings) override;
	void OnRender(RenderContext& rc) override;
	void OnDrawGUI() override;
	MouseCursorMode GetMouseCursorMode() const override { return MouseCursorMode::VisibleFree; }
	bool UsesGameDebugGUI() const override { return false; }
	bool OnRequestExit() override;

  private:
	// ステージ操作
	void CreateStage();

	// ファイル操作
	void New();
	void Open();
	bool LoadStage(const std::filesystem::path& stagePath);
	bool ResolveMissingModels();
	void Save();
	void SaveAs();
	void LoadEditorSettings();
	void SaveEditorSettings() const;
	void UpdateTitle();
	void ErrorMessage(const std::string& message);

	enum class EditorLightType { None, Point, Spot, Area, Directional };
	Light* GetSelectedLight();
	void SelectLight(EditorLightType type, int index);
	void DrawHierarchy();
	void DrawInspector();
	void DrawLightMarkers(const ImVec2& viewportMin, const ImVec2& viewportMax);
	void DuplicateSelectedLight();
	void DeleteSelectedLight();

	// エディタ操作
	void RefreshPropModels();
	void DrawPropBrowser(float height);
	void RenderPropPreview(const RenderContext& rc);
	void RenderDragPreview(const RenderContext& rc);
	bool OpenVmdlEditor(const std::string& modelPath);
	void DrawPlacementDropTarget(const ImVec2& viewportMin, const ImVec2& viewportMax);
	void DrawObjectGizmo(const ImVec2& viewportMin, const ImVec2& viewportMax);
	bool ScreenToTerrainPoint(
		const ImVec2& mousePosition, Vector3& point, Vector3* normal = nullptr) const;
	bool ScreenToPlacementSurface(const ImVec2& mousePosition, Vector3& point,
		Vector3& normal, int ignoredPropIndex = -1) const;
	void SelectObjectAt(
		const ImVec2& mousePosition, const ImVec2& viewportMin, const ImVec2& viewportMax);

	// 編集対象
	std::filesystem::path path;
	std::filesystem::path recentStagePath;
	VSTG data;

	// ステージ参照
	Terrain* terrain = nullptr;
	NavMeshActor* navMesh = nullptr;
	StageLoader* stageLoader = nullptr;
	FreeCameraController* freeCameraController = nullptr;

	// プロップ一覧
	std::vector<std::string> propModelPaths;
	std::unordered_map<std::string, std::shared_ptr<VMDLModel>> propModels;
	std::string propSearch;
	std::string propPreviewRequestPath;
	std::string propPreviewLoadedPath;
	std::shared_ptr<VMDLModel> propPreviewModel;
	std::unique_ptr<RenderTarget> propPreviewTarget;
	std::unique_ptr<Object> propPreviewCameraOwner;
	Camera* propPreviewCamera = nullptr;
	LightManager propPreviewLights;
	VMatRenderParams propPreviewRenderParams;

	// 配置プレビュー
	std::string dragPreviewModelPath;
	std::shared_ptr<VMDLModel> dragPreviewModel;
	Transform dragPreviewTransform;
	VMatRenderParams dragPreviewRenderParams;
	bool showDragPreview = false;
	bool showPlayerStartDragPreview = false;
	bool showLightDragPreview = false;
	Vector3 lightDragPreviewPosition = Vector3::Zero;
	Vector3 playerStartDragPreviewPosition = Vector3::Zero;

	Vector3 effectPlacementPosition = Vector3::Zero;
	std::string placementEffectName;
	bool effectPopupPending = false;

	// 編集状態
	EditorLightType selectedLightType = EditorLightType::None;
	int selectedLightIndex = -1;
	bool lightingSettingsSelected = false;
	int gizmoOperation = 7;
	bool terrainSnapActive = false;
	bool dirty = false;
	size_t cleanStateHash = 0;
	bool maximizeWindowPending = true;
};
