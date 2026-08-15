#pragma once
#include "Resource/VMDLModel.h"

#include <imgui.h>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Core/Foundation/Common.h"
#include "Core/Object/Component.h"
#include "Gameplay/Actor/Actor.h"
#include "nlohmann/json.hpp"
#include "IconsFontAwesome5.h"

class Prop;
class CrystalProp;
class ParticleSystem;
class Spawner;
class Stage;

class StageLoader : public Component
{
  public:
	enum class EditorObjectType
	{
		None,
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
	void SetCrystalBreakParticleSystem(ParticleSystem* particleSystem);
	using SpawnerFactory = std::function<std::shared_ptr<Actor>(const Transform&)>;
	void RegisterSpawnerFactory(const std::string& entityName, SpawnerFactory factory);
	std::vector<Spawner*> GetSpawners() const;
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
	bool BuildEditorPropTransform(
		VMDLModel& model, const Vector3& placementPoint, Transform& transform);
	EditorObjectType GetSelectedEditorObjectType() const { return selectedEditorObjectType; }
	int GetSelectedEditorObjectIndex() const { return selectedEditorObjectIndex; }
	Transform* GetSelectedEditorTransform();
	void RefreshSelectedEditorObject();

  private:
	friend class Prop;
	friend class CrystalProp;
	static constexpr const char* PlacementColliderName = "PLACEMENT";

	struct RigidbodyData
	{
		bool isDynamic = false;
		void DrawGUI()
		{
			if (ImGui::TreeNode((const char*)u8"リジッドボディ設定"))
			{
				ImGui::Checkbox((const char*)u8"動的オブジェクト", &isDynamic);
				ImGui::TreePop();
			}
		}
	};

	enum class PropType
	{
		Standard,
		Crystal
	};

	struct PropData
	{
		std::string name = "Prop";
		std::string tag = "Prop";
		PropType type = PropType::Standard;
		Transform transform = {};
		RigidbodyData rigidbodyData = {};
		std::string modelPath = "";
		bool useDestroy = false;
		float destroyLife = 0.0f;
		uint32_t destroyLayerMask = 0;
		bool isSpawner = false;
		std::string spawnerEntityName = "EnemySmall";

		std::shared_ptr<VMDLModel> model = nullptr;
		bool editorPreview = false;
	};
	std::vector<PropData> propDataList = {};
	void DrawDestroyGUI(PropData& propData);
	void DrawEditorGUI();
	Actor* CreatePropActor(PropData& propData);
	void ConfigureSpawner(Actor* actor, const PropData& propData);
	std::shared_ptr<VMDLModel> LoadPropModel(const std::string& modelPath) const;
	Vector3 GetPropPlacementOffset(VMDLModel& model);
	static uint32_t GetDefaultDestroyLayerMask();

	std::filesystem::path jsonPath = {};
	std::string jsonText;
	Stage* stage = nullptr;
	ParticleSystem* crystalBreakParticleSystem = nullptr;
	std::vector<Actor*> addedRealActors = {};
	std::vector<Actor*> addedPropActors = {};
	std::unordered_map<std::string, SpawnerFactory> spawnerFactories = {};
	const std::unordered_map<std::string, std::shared_ptr<VMDLModel>>* editorModels = nullptr;
	std::unordered_map<std::string, DirectX::BoundingBox> editorSelectionBounds;
	EditorObjectType selectedEditorObjectType = EditorObjectType::None;
	int selectedEditorObjectIndex = -1;
	Transform selectedEditorTransform;
};
