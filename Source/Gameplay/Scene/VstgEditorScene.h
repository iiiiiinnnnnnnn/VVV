#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <utility>

#include "Gameplay/Lighting/LightManager.h"
#include "Gameplay/Scene/EditorScene.h"
#include "Resource/VSTG.h"
#include "Rendering/Core/PBRRenderParams.h"
#include "Rendering/Component/Terrain.h"

class StageLoader;
class Terrain;
class NavMeshActor;
class FreeCameraController;
class Camera;
class Object;
class RenderTarget;
class DepthTarget;
class VMDLModel;
class Texture;

class VstgEditorScene : public EditorScene
{
  public:
	VstgEditorScene();
	~VstgEditorScene() override;

  protected:
	SerialDocument* PrepareSerialDocument() override;
	void OnSerialDocumentChanged() override;
	void OnInit() override;
	void OnUninit() override;
	void OnUpdate() override;
	void ConfigureRenderSettings(RenderSettings& settings) override;
	void OnRender(RenderContext& rc) override;
	void OnDrawGUI() override;
	bool UsesGameDebugGUI() const override { return false; }
	bool OnRequestExit() override;

  private:
	void HandleShortcuts();
	void DrawTerrainGUI();
	void DrawTerrainLayerGUI();
	void DrawBrushGUI();
	void RefreshTerrainAssets();
	void UpdateTerrainBrush();
	bool ScreenToTerrainUV(float& u, float& v) const;
	void ApplyBrush(float u, float v, float heightSign);
	bool AddBrushTexture(const std::string& filename);
	bool SetBrushTexture(int index);
	float GetTerrainLayerValue(int index) const;
	float SampleBrushMask(float u, float v) const;

	enum class BrushMode { RaiseLower, SetHeight, Paint, GrassPaint, NavMeshPaint };
	struct TerrainBrush
	{
		std::string name;
		std::string filepath;
		int width = 0;
		int height = 0;
		std::vector<float> mask;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderResourceView;
	};
	const TerrainBrush* GetCurrentBrush() const;
	struct TerrainLayerImage
	{
		std::string name;
		std::string baseColorPath;
		std::string normalPath;
		std::shared_ptr<Texture> texture;
	};
	std::vector<TerrainBrush> brushes;
	std::vector<TerrainLayerImage> terrainLayerImages;
	int currentBrushIndex = -1;
	int currentTerrainLayerIndex = 0;
	bool use_brush = false;
	bool brushViewportInputAllowed = false;
	BrushMode brushMode = BrushMode::RaiseLower;
	int brush_size = 32;
	float heightBrushStrength = 0.02f;
	float setHeightValue = 0.0f;
	float paintOpacity = 0.08f;
	bool invertBrushMask = false;
	std::string terrainEditorMessage;
	TerrainGrassRenderer::Settings grassDraftSettings;
	bool grassDraftInitialized = false;
	TerrainBrushPreview terrainBrushPreview;

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
	void DrawDecalPalette();
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

	// デカール画像一覧（編集用）
	std::vector<std::pair<std::string, std::shared_ptr<Texture>>> decalTextures;
	bool decalTexturesLoaded = false;
	std::string selectedDecalTexturePath;

	// プロップ一覧
	std::vector<std::string> propModelPaths;
	std::unordered_map<std::string, std::shared_ptr<VMDLModel>> propModels;
	std::string propSearch;
	std::string propPreviewRequestPath;
	std::string propPreviewLoadedPath;
	std::shared_ptr<VMDLModel> propPreviewModel;
	std::unique_ptr<RenderTarget> propPreviewTarget;
	std::unique_ptr<DepthTarget> propPreviewDepthTarget;
	std::unique_ptr<Object> propPreviewCameraOwner;
	Camera* propPreviewCamera = nullptr;
	LightManager propPreviewLights;
	PBRRenderParams propPreviewRenderParams;

	// 配置プレビュー
	std::string dragPreviewModelPath;
	std::shared_ptr<VMDLModel> dragPreviewModel;
	Transform dragPreviewTransform;
	PBRRenderParams dragPreviewRenderParams;
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
