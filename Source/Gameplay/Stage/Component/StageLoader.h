// StageLoader.h

#pragma once
#include "Resource/VMDLModel.h"
#include "Rendering/Core/ShaderParam.h"

#include <imgui.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "Core/Foundation/Common.h"
#include "Core/Object/Component.h"
#include "Gameplay/Actor/Actor.h"
#include "nlohmann/json.hpp"
#include "IconsFontAwesome5.h"

class Prop;
class CrystalProp;
class ParticleSystem;
class Stage;

class StageLoader : public Component
{
public:
	StageLoader(Object* owner, Stage* stage, std::filesystem::path jsonPath);
	StageLoader(Object* owner, Stage* stage, const std::string& jsonText, bool fromMemory);
	~StageLoader() = default;

	void Update() override;
	void Render(const RenderContext& rc) override;
	void DrawGUI() override;
	const char* GetDebugName() const override { return ICON_FA_BOX " StageLoader"; }

	void LoadJson();
	void LoadJsonText(const std::string& text);
	void SaveJson();
	std::string SaveJsonText();
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

		std::shared_ptr<VMDLModel> model = nullptr;
		ShaderParamListWithMaterialName shaderParams = {};

	} addPropData = {};
	std::vector<PropData> propDataList = {};

	struct CrystalData
	{
		Transform transform = {};
	};

	struct CrystalShaderData
	{
		Color color = Color(0.05f, 0.45f, 0.9f, 1.0f);
		Color emission = Color(0.0f, 0.55f, 1.0f, 0.25f);
		Color fresnelColor = Color(1.0f, 1.0f, 1.0f, 1.0f);
		float fresnelPower = 1.5f;
		float fresnelStrength = 1.0f;
		float metallic = 0.0f;
		float roughness = 0.0f;
		float occlusion = 1.0f;
		float occlusionStrength = 1.0f;
		float shadowStrength = 1.0f;
		bool isFlatShading = false;

		ShaderParamList MakePBRParams() const;
	};

	CrystalData addCrystalData = {};
	CrystalShaderData crystalShaderData = {};
	ShaderParamListWithMaterialName crystalShaderParams = {};
	std::vector<CrystalData> crystalDataList = {};
	void DrawPBRParamsGUI(PropData& propData);
	void DrawColliderTypeGUI(PropData& propData);
	void DrawDestroyGUI(PropData& propData);
	void DrawCrystalDataGUI(CrystalData& crystalData);
	void DrawCrystalShaderParamsGUI();

	std::filesystem::path jsonPath = {};
	std::string jsonText;
	Stage* stage = nullptr;
	ParticleSystem* crystalBreakParticleSystem = nullptr;
	std::vector<Actor*> addedRealActors = {};
	std::vector<Prop*> addedPropActors = {};
	std::vector<CrystalProp*> addedCrystalActors = {};
};







