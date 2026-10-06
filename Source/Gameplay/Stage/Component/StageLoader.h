// StageLoader.h
#pragma once
#include "Resource/VMDLModel.h"

#include <imgui.h>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Core/Foundation/Common.h"
#include "Core/Object/Component.h"
#include "Gameplay/Actor/Actor.h"
#include "Rendering/Renderer/WaterRenderer.h"
#include "nlohmann/json.hpp"
#include "IconsFontAwesome5.h"

class Spawner;
class Stage;
class Water;

class StageLoader : public Component
{
  public:
	enum class EditorObjectType
	{
		None,
		WorldWater,
		PlayerStart,
		BlockedArea,
		Prop
	};

	struct EditorObjectReference
	{
		EditorObjectType type = EditorObjectType::None;
		int index = -1;
		Transform* transform = nullptr;
	};

	StageLoader(Object* owner, Stage* stage, std::filesystem::path jsonPath);
	StageLoader(Object* owner, Stage* stage, const std::string& jsonText, bool fromMemory);
	~StageLoader() = default;

	void Update() override;
	void DrawGUI() override;
	const char* GetDebugName() const override { return ICON_FA_BOX " StageLoader"; }

	void LoadJson();
	void LoadJsonText(const std::string& text);
	void SaveJson();
	std::string SaveJsonText();
	const std::string& GetError() const { return error; }
	using SpawnerFactory = std::function<std::shared_ptr<Actor>(const Transform&, const std::string&)>;
	void RegisterSpawnerFactory(const std::string& entityName, SpawnerFactory factory);
	std::vector<Spawner*> GetSpawners(std::string_view entityName = {}) const;
	void SetEditorModels(
		const std::unordered_map<std::string, std::shared_ptr<VMDLModel>>* models)
	{
		editorModels = models;
		editorSelectionBounds.clear();
	}
	std::vector<EditorObjectReference> GetEditorObjects();
	bool SelectEditorObject(EditorObjectType type, int index);
	bool SelectEditorActor(const Actor* actor);
	bool SelectEditorObjectAtRay(const Vector3& origin, const Vector3& direction);
	void ClearEditorSelection();
	bool AddEditorProp(const std::string& modelPath, const Vector3& terrainPoint);
	void SetEditorPlayerStart(const Vector3& terrainPoint);
	void AddEditorBlockedArea();
	bool HasPlayerStart() const { return !playerStartTransforms.empty(); }
	const std::vector<Transform>& GetPlayerStartTransforms() const { return playerStartTransforms; }
	const Transform& GetRandomPlayerStartTransform() const;
	std::vector<std::string> GetMissingModelPaths() const;
	bool ReplaceMissingModelPath(
		const std::string& missingPath, const std::string& replacementPath);
	void RemovePropsWithModelPath(const std::string& modelPath);
	bool BuildEditorPropTransform(
		VMDLModel& model, const Vector3& placementPoint, Transform& transform);
	EditorObjectType GetSelectedEditorObjectType() const { return selectedEditorObjectType; }
	int GetSelectedEditorObjectIndex() const { return selectedEditorObjectIndex; }
	Transform* GetSelectedEditorTransform();
	void RefreshSelectedEditorObject();

  private:
	static constexpr const char* PlacementColliderName = "PLACEMENT";

	struct PropData
	{
		Transform transform = {};
		std::string modelPath = "";
		bool initialSpawned = false;

		std::shared_ptr<VMDLModel> model = nullptr;
		bool editorPreview = false;
	};

	struct WorldWaterData
	{
		bool enabled = false;
		Transform transform;
		WaterRenderer::Settings settings;

		WorldWaterData()
		{
			transform.scale = {1000.0f, 1.0f, 1000.0f};
			transform.Update();
		}
	};
	struct BlockedAreaData
	{
		std::string name = "Blocked Area";
		Transform transform;
	};
	std::vector<PropData> propDataList = {};
	std::vector<Transform> playerStartTransforms;
	std::vector<BlockedAreaData> blockedAreas;
	void DrawEditorGUI();
	void DrawWorldWaterEditor();
	bool DrawPropEditor(int index);
	Actor* CreatePropActor(PropData& propData);
	void ApplyPropData(Actor* actor, PropData& propData);
	Actor* CreateBlockedAreaActor(BlockedAreaData& area);
	void ApplyBlockedAreaData(int index);
	void CreateWorldWaterActor();
	void ApplyWorldWaterData();
	void ConfigureSpawner(Actor* actor, const PropData& propData);
	bool ValidateModelName(const std::string& modelPath, const std::string& replacedPath = {});
	std::shared_ptr<VMDLModel> LoadPropModel(const std::string& modelPath) const;
	Vector3 GetPropPlacementOffset(VMDLModel& model);

	std::filesystem::path jsonPath = {};
	std::string jsonText;
	std::string error;
	Stage* stage = nullptr;
	WorldWaterData worldWater;
	Water* worldWaterActor = nullptr;
	std::vector<Actor*> addedRealActors = {};
	std::vector<Actor*> addedPropActors = {};
	std::vector<Actor*> blockedAreaActors;
	std::unordered_map<std::string, SpawnerFactory> spawnerFactories = {};
	const std::unordered_map<std::string, std::shared_ptr<VMDLModel>>* editorModels = nullptr;
	std::unordered_map<std::string, DirectX::BoundingBox> editorSelectionBounds;
	EditorObjectType selectedEditorObjectType = EditorObjectType::None;
	int selectedEditorObjectIndex = -1;
	Transform selectedEditorTransform;
};
