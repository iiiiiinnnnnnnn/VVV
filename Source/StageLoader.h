// StageLoader.h

#pragma once

#include "Common.h"
#include "Component.h"
#include "Actor.h"
#include "nlohmann/json.hpp"
#include "IconsFontAwesome5.h"

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

private:
	friend class Prop;

	enum class AddType
	{
		Spawner,
		Prop
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

	struct PropData
	{
		// •Û‘¶‚·‚é
		Transform transform = {};
		BoxColliderData boxColliderData = {};
		RigidbodyData rigidbodyData = {};
		std::string modelPath = "";
		float metallic = 0.0f;
		float roughness = 0.5f;
		float occlusion = 1.0f;
		float occlusionStrength = 1.0f;

		// •Û‘¶‚µ‚È‚¢
		std::shared_ptr<Model> model = nullptr;
		ShaderParamListWithMaterialName shaderParams = {};

	} addPropData = {};
	std::vector<PropData> propDataList = {};

	std::filesystem::path jsonPath = {};
	Actor* stage = nullptr;
	std::vector<Actor*> addedRealActors = {};
};
