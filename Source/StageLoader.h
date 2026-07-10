// StageLoader.h

#pragma once
#include "Model.h"
#include "ShaderParam.h"

#include <imgui.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "Common.h"
#include "Component.h"
#include "Actor.h"
#include "nlohmann/json.hpp"
#include "IconsFontAwesome5.h"

class Prop;
class CrystalProp;
class ParticleSystem;

class StageLoader : public Component
{
public:
	StageLoader(Object* owner, Actor* stage, std::filesystem::path jsonPath);
	~StageLoader() = default;

	void Update() override;
	void Render(const RenderContext& rc) override;
	void DrawGUI() override;
	const char* GetDebugName() const override { return ICON_FA_BOX " StageLoader"; }

	void LoadJson();
	void SaveJson();
	void SetCrystalBreakParticleSystem(ParticleSystem* particleSystem);

private:
	friend class Prop;
	friend class CrystalProp;

	enum class AddType
	{
		Spawner,
		Prop,
		Crystal
	} addType = AddType::Spawner;

	struct RigidbodyData
	{
		bool isDynamic = false;
		void DrawGUI()
		{
			if (ImGui::TreeNode(ICON_FA_WEIGHT_HANGING " Rigidbody Data"))
			{
				ImGui::Checkbox("Is Dynamic", &isDynamic);
				ImGui::TreePop();
			}
		}
	};

	struct BoxColliderData
	{
		Vector3 size = Vector3::One;
		Vector3 localPosition = Vector3::Zero;
		float staticFriction = 0.5f;
		float dynamicFriction = 0.5f;
		float restitution = 0.1f;
		void DrawGUI()
		{
			if (ImGui::TreeNode(ICON_FA_SHAPES " BoxCollider Data"))
			{
				ImGui::DragFloat3("Size", &size.x, 0.01f);
				ImGui::DragFloat3("Local Position", &localPosition.x, 0.01f);
				ImGui::DragFloat("Static Friction", &staticFriction, 0.01f, 0.0f, 1.0f);
				ImGui::DragFloat("Dynamic Friction", &dynamicFriction, 0.01f, 0.0f, 1.0f);
				ImGui::DragFloat("Restitution", &restitution, 0.01f, 0.0f, 1.0f);
				ImGui::TreePop();
			}
		}
	};

	struct SpawnerData
	{
		// •Û‘¶‚·‚é
		Transform transform = {};
		BoxColliderData boxColliderData = {};
		enum class SpawnerType
		{
			Aracore

		} spawnerType = SpawnerType::Aracore;

	} addSpawnerData = {};
	std::vector<SpawnerData> spawnerDataList = {};

	enum class ColliderType
	{
		Box,
		Mesh
	};

	struct PropData
	{
		// •Û‘¶‚·‚é
		Transform transform = {};
		BoxColliderData boxColliderData = {};
		RigidbodyData rigidbodyData = {};
		ColliderType colliderType = ColliderType::Box;
		std::string modelPath = "";
		Color color = Color(1.0f, 1.0f, 1.0f, 1.0f);
		Color emission = Color(0.0f, 0.0f, 0.0f, 0.0f);
		float metallic = 0.0f;
		float roughness = 0.5f;
		float occlusion = 1.0f;
		float occlusionStrength = 1.0f;
		float shadowStrength = 1.0f;
		bool isFlatShading = false;
		bool useDestroy = false;
		float destroyLife = 0.0f;

		ShaderParamList MakePBRParams() const;

		// •Û‘¶‚µ‚È‚¢
		std::shared_ptr<Model> model = nullptr;
		ShaderParamListWithMaterialName shaderParams = {};

	} addPropData = {};
	std::vector<PropData> propDataList = {};

	struct CrystalData
	{
		// ????
		std::string modelPath = "Data/Model/Prop/crystal.glb";
		Transform parentTransform = {};
		int count = 1;
		std::vector<Transform> transforms = {{}};
		Color color = Color(0.45f, 0.85f, 1.0f, 0.45f);
		Color emission = Color(0.0f, 0.0f, 0.0f, 0.0f);
		float metallic = 0.0f;
		float roughness = 0.5f;
		float occlusion = 1.0f;
		float occlusionStrength = 1.0f;
		float shadowStrength = 1.0f;
		bool isFlatShading = true;

		ShaderParamList MakePBRParams() const;

		// ?????
		std::vector<std::shared_ptr<Model>> models = {};
		ShaderParamListWithMaterialName shaderParams = {};
	};

	CrystalData addCrystalData = {};
	std::vector<CrystalData> crystalDataList = {};

	void DrawPBRParamsGUI(PropData& propData);
	void DrawColliderTypeGUI(PropData& propData);
	void DrawDestroyGUI(PropData& propData);
	void DrawCrystalDataGUI(CrystalData& crystalData);

	std::filesystem::path jsonPath = {};
	Actor* stage = nullptr;
	ParticleSystem* crystalBreakParticleSystem = nullptr;
	std::vector<Actor*> addedRealActors = {};
	std::vector<Prop*> addedPropActors = {};
	std::vector<CrystalProp*> addedCrystalActors = {};
};

