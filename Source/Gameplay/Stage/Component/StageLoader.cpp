// StageLoader.cpp

#include "Gameplay/Stage/Component/StageLoader.h"
#include "Gameplay/Stage/Stage.h"
#include "Rendering/Core/Graphics.h"
#include "magic_enum/magic_enum.hpp"
#include <fstream>
#include <iomanip>
#include <sstream>
#include "Resource/ResourceManager.h"
#include "Application/Time/GameTime.h"
#include "Gameplay/Actor/ActorManager.h"
#include "Gameplay/Actor/Prop.h"
#include "Gameplay/Actor/CrystalProp.h"

using json = nlohmann::json;

static void LoadTransformJson(const json& transformJson, Transform& transform)
{
	if (transformJson.contains("position"))
	{
		transform.position.x = transformJson["position"].value("x", 0.0f);
		transform.position.y = transformJson["position"].value("y", 0.0f);
		transform.position.z = transformJson["position"].value("z", 0.0f);
	}

	if (transformJson.contains("rotation"))
	{
		transform.rotation.x = transformJson["rotation"].value("x", 0.0f);
		transform.rotation.y = transformJson["rotation"].value("y", 0.0f);
		transform.rotation.z = transformJson["rotation"].value("z", 0.0f);
		transform.rotation.w = transformJson["rotation"].value("w", 1.0f);
	}

	if (transformJson.contains("scale"))
	{
		transform.scale.x = transformJson["scale"].value("x", 1.0f);
		transform.scale.y = transformJson["scale"].value("y", 1.0f);
		transform.scale.z = transformJson["scale"].value("z", 1.0f);
	}
}

static void SaveTransformJson(json& transformJson, const Transform& transform)
{
	transformJson["position"]["x"] = transform.position.x;
	transformJson["position"]["y"] = transform.position.y;
	transformJson["position"]["z"] = transform.position.z;

	transformJson["rotation"]["x"] = transform.rotation.x;
	transformJson["rotation"]["y"] = transform.rotation.y;
	transformJson["rotation"]["z"] = transform.rotation.z;
	transformJson["rotation"]["w"] = transform.rotation.w;

	transformJson["scale"]["x"] = transform.scale.x;
	transformJson["scale"]["y"] = transform.scale.y;
	transformJson["scale"]["z"] = transform.scale.z;
}

ShaderParamList StageLoader::PropData::MakePBRParams() const
{
	return {
		{"color", color},
		{"emission", emission},
		{"metalness", metallic},
		{"roughness", roughness},
		{"occlusion", occlusion},
		{"occlusionStrength", occlusionStrength},
		{"shadowStrength", shadowStrength},
		{"IsFlatShading", isFlatShading},
	};
}

void StageLoader::DrawCrystalDataGUI(CrystalData& crystalData)
{
	crystalData.transform.DrawGUI();
}

void StageLoader::DrawPBRParamsGUI(PropData& propData)
{
	if (ImGui::TreeNode("PBR Shader Params"))
	{
		ImGui::ColorEdit4("Color", &propData.color.x);
		ImGui::ColorEdit4("Emission", &propData.emission.x);
		ImGui::DragFloat("Metallic", &propData.metallic, 0.01f, 0.0f, 1.0f);
		ImGui::DragFloat("Roughness", &propData.roughness, 0.01f, 0.0001f, 1.0f);
		ImGui::DragFloat("Occlusion", &propData.occlusion, 0.01f, 0.0f, 1.0f);
		ImGui::DragFloat("Occlusion Strength", &propData.occlusionStrength, 0.01f, 0.0f, 1.0f);
		ImGui::DragFloat("Shadow Strength", &propData.shadowStrength, 0.01f, 0.0f, 1.0f);
		ImGui::Checkbox("Is Flat Shading", &propData.isFlatShading);
		ImGui::TreePop();
	}
}

void StageLoader::DrawColliderTypeGUI(PropData& propData)
{
	std::string previewName = std::string(magic_enum::enum_name(propData.colliderType));
	if (ImGui::BeginCombo("Collider Type", previewName.c_str()))
	{
		for (ColliderType type : magic_enum::enum_values<ColliderType>())
		{
			std::string name = std::string(magic_enum::enum_name(type));
			bool isSelected = propData.colliderType == type;
			if (ImGui::Selectable(name.c_str(), isSelected))
				propData.colliderType = type;
			if (isSelected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
}

void StageLoader::DrawDestroyGUI(PropData& propData)
{
	ImGui::Checkbox("Use Destroy", &propData.useDestroy);
	if (propData.useDestroy)
	{
		ImGui::DragFloat("Destroy Life", &propData.destroyLife, 0.1f, 0.0f, 100000.0f);
		if (propData.destroyLife < 0.0f) propData.destroyLife = 0.0f;
	}
}
StageLoader::StageLoader(Object* owner, Stage* stage, std::filesystem::path jsonPath)
	: Component(owner), stage(stage), jsonPath(jsonPath)
{
	LoadJson();
}

StageLoader::StageLoader(Object* owner, Stage* stage, const std::string& jsonText, bool fromMemory)
	: Component(owner), stage(stage), jsonText(jsonText)
{
	LoadJson();
}

void StageLoader::Update()
{
	for (int propIndex = 0; propIndex < static_cast<int>(propDataList.size()); ++propIndex)
	{
		auto& prop = propDataList[propIndex];
		if (!prop.model)
		{
			prop.model = ResourceManager::Instance().LoadModel(prop.modelPath);
		}

		ShaderParamList param = prop.MakePBRParams();
		ModelRenderer::SetShaderParamForAllMaterials(prop.model.get(), param, prop.shaderParams);

		prop.transform.Update();
		prop.model->UpdateTransform(prop.transform.matrix);

		if (propIndex < static_cast<int>(addedPropActors.size()) && addedPropActors[propIndex])
		{
			addedPropActors[propIndex]->ApplyStageData(prop);
		}
	}

	for (int crystalIndex = 0; crystalIndex < static_cast<int>(crystalDataList.size()); ++crystalIndex)
	{
		auto& crystal = crystalDataList[crystalIndex];
		if (crystalIndex < static_cast<int>(addedCrystalActors.size()) && addedCrystalActors[crystalIndex])
		{
			addedCrystalActors[crystalIndex]->ApplyStageData(crystal);
		}
	}}

void StageLoader::Render(const RenderContext& rc)
{
	if (!showDebug) return;

	for (auto& prop : propDataList)
	{
		Game::Graphics::Instance().GetModelRenderer()->Draw(ModelShaderId::PBR, prop.model, prop.shaderParams);
	}

	for (auto& spawner : spawnerDataList)
	{
		Matrix world = spawner.transform.matrix * Matrix::CreateTranslation(spawner.boxColliderData.localPosition);
		Game::Graphics::Instance().GetShapeRenderer()->DrawBox(
			world.Translation(), spawner.transform.rotation.ToEuler(), spawner.boxColliderData.size, {1, 1, 1});
	}
	for (auto& prop : propDataList)
	{
		if (prop.colliderType != ColliderType::Box) continue;

		Matrix world = prop.transform.matrix * Matrix::CreateTranslation(prop.boxColliderData.localPosition);
		Game::Graphics::Instance().GetShapeRenderer()->DrawBox(
			world.Translation(), prop.transform.rotation.ToEuler(), prop.boxColliderData.size, {1, 1, 1});
	}
}

void StageLoader::SetCrystalBreakParticleSystem(ParticleSystem* particleSystem)
{
	crystalBreakParticleSystem = particleSystem;

	for (CrystalProp* crystalActor : addedCrystalActors)
	{
		if (crystalActor)
		{
			crystalActor->SetDestroyedCallback([this](CrystalProp* destroyedCrystal)
				{
					for (Actor*& addedActor : addedRealActors)
					{
						if (addedActor == destroyedCrystal) addedActor = nullptr;
					}
					for (CrystalProp*& addedActor : addedCrystalActors)
					{
						if (addedActor == destroyedCrystal) addedActor = nullptr;
					}
				});
				crystalActor->SetBreakParticleSystem(crystalBreakParticleSystem);
		}
	}
}
void StageLoader::DrawGUI()
{
	DrawEditorGUI(false);
}

void StageLoader::DrawPropGUI()
{
	addType = AddType::Prop;
	DrawEditorGUI(true);
}

void StageLoader::DrawSpawnerGUI()
{
	addType = AddType::Spawner;
	DrawEditorGUI(true);
}

void StageLoader::DrawCrystalGUI()
{
	addType = AddType::Crystal;
	DrawEditorGUI(true);
}

void StageLoader::DrawEditorGUI(bool singleSection)
{
	ImGui::PushID(static_cast<int>(addType));
	if (!singleSection)
	{
		ImGui::Text("Json Path: %s", jsonPath.string().c_str());
		float availWidth = ImGui::GetContentRegionAvail().x;
		float spacing = ImGui::GetStyle().ItemSpacing.x;
		float buttonWidth = (availWidth - spacing) * 0.5f;

		if (ImGui::Button("Save", ImVec2(buttonWidth, 0.0f)))
		{
			SaveJson();
		}

		ImGui::SameLine();

		if (ImGui::Button("Reload", ImVec2(buttonWidth, 0.0f)))
		{
			LoadJson();
		}

		ImGui::Separator();
	}


	ImGui::Text("Add Object:");
	if (ImGui::BeginChild("##addwin", ImVec2(0.0f, 400.0f), true))
	{

		if (!singleSection)
		{
			using AddType = decltype(addType);
			std::string previewName = std::string(magic_enum::enum_name(addType));
			if (ImGui::BeginCombo("##StageLoaderCombo", previewName.c_str()))
			{
				for (AddType type : magic_enum::enum_values<AddType>())
				{
					std::string name = std::string(magic_enum::enum_name(type));
					bool isSelected = addType == type;
					if (ImGui::Selectable(name.c_str(), isSelected)) addType = type;

					if (isSelected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
		}


		if (addType == AddType::Spawner)
		{
			// transform
			addSpawnerData.transform.DrawGUI();

			// boxCollider
			addSpawnerData.boxColliderData.DrawGUI();

			ImGui::Text("Spawner Type:");
			if (ImGui::BeginCombo("##StageLoaderSpawnerType", std::string(magic_enum::enum_name(addSpawnerData.spawnerType)).c_str()))
			{
				for (auto type : magic_enum::enum_values<decltype(addSpawnerData.spawnerType)>())
				{
					std::string name = std::string(magic_enum::enum_name(type));
					bool isSelected = (addSpawnerData.spawnerType == type);
					if (ImGui::Selectable(name.c_str(), isSelected))
					{
						addSpawnerData.spawnerType = type;
					}
					if (isSelected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			if (ImGui::Button((const char*)u8"Add to loader", ImVec2(-FLT_MIN, 30.0f)))
			{
				spawnerDataList.push_back(addSpawnerData);
			}

		}
		else if (addType == AddType::Prop)
		{
			// transform
			addPropData.transform.DrawGUI();

			DrawColliderTypeGUI(addPropData);
			if (addPropData.colliderType == ColliderType::Box)
			{
				// boxCollider
				addPropData.boxColliderData.DrawGUI();

				// rigidbody
				addPropData.rigidbodyData.DrawGUI();
			}

			DrawPBRParamsGUI(addPropData);
			DrawDestroyGUI(addPropData);


			std::vector<std::filesystem::path> modelFiles;
			for (const auto& file : std::filesystem::directory_iterator("Data/Model/Prop"))
			{
				if (file.path().extension() == ".vmdl")
				{
					modelFiles.push_back(file.path());
				}
			}

			ImGui::Text("VMDLModel Path:");
			if (ImGui::BeginCombo("##StageLoaderPropDetail", std::filesystem::path(addPropData.modelPath).filename().string().c_str()))
			{
				for (const auto& path : modelFiles)
				{
					std::string name = path.stem().string();
					std::string modelPath = "Data/Model/Prop/" + name;
					bool isSelected = (addPropData.modelPath == modelPath);
					if (ImGui::Selectable(name.c_str(), isSelected))
					{
						addPropData.modelPath = modelPath;
					}
					if (isSelected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}


			if (ImGui::Button((const char*)u8"Add to loader", ImVec2(-FLT_MIN, 30.0f)))
			{
				std::filesystem::path cachedModelPath(addPropData.modelPath);
				cachedModelPath.replace_extension(".vmdl");
				if (std::filesystem::exists(cachedModelPath))
				{
					propDataList.push_back(addPropData);
					auto propActor = std::make_shared<Prop>(propDataList.back());
					addedRealActors.push_back(propActor.get());
					addedPropActors.push_back(propActor.get());
					stage->GetActorManager().Register(propActor);
				}
			}
		}
		else if (addType == AddType::Crystal)
		{
			DrawCrystalDataGUI(addCrystalData);

			if (ImGui::Button((const char*)u8"Add to loader", ImVec2(-FLT_MIN, 30.0f)))
			{
				if (true)
				{
					crystalDataList.push_back(addCrystalData);
					auto crystalActor = std::make_shared<CrystalProp>(crystalDataList.back());
                    crystalActor->SetDestroyedCallback([this](CrystalProp* destroyedCrystal)
				{
					for (Actor*& addedActor : addedRealActors)
					{
						if (addedActor == destroyedCrystal) addedActor = nullptr;
					}
					for (CrystalProp*& addedActor : addedCrystalActors)
					{
						if (addedActor == destroyedCrystal) addedActor = nullptr;
					}
				});
				crystalActor->SetBreakParticleSystem(crystalBreakParticleSystem);
					addedRealActors.push_back(crystalActor.get());
					addedCrystalActors.push_back(crystalActor.get());
					stage->GetActorManager().Register(crystalActor);
				}
			}
		}
	}

	ImGui::EndChild();

	if (!singleSection || addType == AddType::Spawner)
	{
	ImGui::TextUnformatted("SpawnerList:");

	ImGui::BeginChild("##spawnerlistwin", ImVec2(0.0f, 150.0f), true);

	for (int i = 0; i < static_cast<int>(spawnerDataList.size()); ++i)
	{
		auto& spawnerData = spawnerDataList[i];

		ImGui::PushID(i);

		if (ImGui::TreeNode("Spawner"))
		{
			spawnerData.transform.DrawGUI();
			spawnerData.boxColliderData.DrawGUI();

			std::string spawnerTypeName = std::string(magic_enum::enum_name(spawnerData.spawnerType));
			ImGui::Text("Spawner Type: %s", spawnerTypeName.c_str());

			if (ImGui::Button("Remove"))
			{
				spawnerDataList.erase(spawnerDataList.begin() + i);
				ImGui::TreePop();
				ImGui::PopID();
				break;
			}

			ImGui::TreePop();
		}

		ImGui::PopID();
	}

	ImGui::EndChild();
	}

	if (!singleSection || addType == AddType::Prop)
	{
	ImGui::TextUnformatted("PropList:");

	ImGui::BeginChild("##proplistwin", ImVec2(0.0f, 150.0f), true);

	for (int i = 0; i < static_cast<int>(propDataList.size()); ++i)
	{
		auto& propData = propDataList[i];

		ImGui::PushID(i);

		if (ImGui::TreeNode("Prop"))
		{
			propData.transform.DrawGUI();
			DrawColliderTypeGUI(propData);
			if (propData.colliderType == ColliderType::Box)
			{
				propData.boxColliderData.DrawGUI();
				propData.rigidbodyData.DrawGUI();
			}

			DrawPBRParamsGUI(propData);
			DrawDestroyGUI(propData);

			ImGui::Text("VMDLModel Path: %s", propData.modelPath.c_str());

			if (ImGui::Button("Remove"))
			{
				if (i < static_cast<int>(addedPropActors.size()) && addedPropActors[i])
				{
					addedPropActors[i]->Destroy();
				}
				if (i < static_cast<int>(addedRealActors.size())) addedRealActors.erase(addedRealActors.begin() + i);
				if (i < static_cast<int>(addedPropActors.size())) addedPropActors.erase(addedPropActors.begin() + i);
				propDataList.erase(propDataList.begin() + i);
				ImGui::TreePop();
				ImGui::PopID();
				break;
			}

			ImGui::TreePop();
		}

		ImGui::PopID();
	}

	ImGui::EndChild();
	}

	if (!singleSection || addType == AddType::Crystal)
	{
	ImGui::TextUnformatted("CrystalList:");

	ImGui::BeginChild("##crystallistwin", ImVec2(0.0f, 150.0f), true);

	for (int i = 0; i < static_cast<int>(crystalDataList.size()); ++i)
	{
		auto& crystalData = crystalDataList[i];

		ImGui::PushID(i);

		if (ImGui::TreeNode("CrystalProp"))
		{
			DrawCrystalDataGUI(crystalData);

			if (ImGui::Button("Duplicate"))
			{
				CrystalData copiedCrystalData = crystalData;
				crystalDataList.insert(crystalDataList.begin() + i + 1, copiedCrystalData);

				auto crystalActor = std::make_shared<CrystalProp>(crystalDataList[i + 1]);
                crystalActor->SetDestroyedCallback([this](CrystalProp* destroyedCrystal)
				{
					for (Actor*& addedActor : addedRealActors)
					{
						if (addedActor == destroyedCrystal) addedActor = nullptr;
					}
					for (CrystalProp*& addedActor : addedCrystalActors)
					{
						if (addedActor == destroyedCrystal) addedActor = nullptr;
					}
				});
				crystalActor->SetBreakParticleSystem(crystalBreakParticleSystem);
				const int realActorIndex = static_cast<int>(addedPropActors.size()) + i + 1;
				if (realActorIndex <= static_cast<int>(addedRealActors.size()))
				{
					addedRealActors.insert(addedRealActors.begin() + realActorIndex, crystalActor.get());
				}
				else
				{
					addedRealActors.push_back(crystalActor.get());
				}

				if (i + 1 <= static_cast<int>(addedCrystalActors.size()))
				{
					addedCrystalActors.insert(addedCrystalActors.begin() + i + 1, crystalActor.get());
				}
				else
				{
					addedCrystalActors.push_back(crystalActor.get());
				}

				stage->GetActorManager().Register(crystalActor);
				ImGui::TreePop();
				ImGui::PopID();
				break;
			}
			ImGui::SameLine();

			if (ImGui::Button("Remove"))
			{
				if (i < static_cast<int>(addedCrystalActors.size()) && addedCrystalActors[i])
				{
					addedCrystalActors[i]->Destroy();
				}
				const int realActorIndex = static_cast<int>(addedPropActors.size()) + i;
				if (realActorIndex < static_cast<int>(addedRealActors.size())) addedRealActors.erase(addedRealActors.begin() + realActorIndex);
				if (i < static_cast<int>(addedCrystalActors.size())) addedCrystalActors.erase(addedCrystalActors.begin() + i);
				crystalDataList.erase(crystalDataList.begin() + i);
				ImGui::TreePop();
				ImGui::PopID();
				break;
			}

			ImGui::TreePop();
		}

		ImGui::PopID();
	}

	ImGui::EndChild();
	}
	ImGui::PopID();
}

void StageLoader::LoadJson()
{
	std::ifstream file;
	std::istringstream memory(jsonText);
	std::istream* input = &memory;
	if (jsonText.empty())
	{
		if (!std::filesystem::exists(jsonPath)) return;
		file.open(jsonPath);
		if (!file) return;
		input = &file;
	}

	json root;

	try
	{
		*input >> root;
	}
	catch (const json::parse_error&)
	{
		return;
	}

	spawnerDataList.clear();
	propDataList.clear();
	crystalDataList.clear();

	for (auto addedActor : addedRealActors)
	{
		if (addedActor) addedActor->Destroy();
	}
	addedRealActors.clear();
	addedPropActors.clear();
	addedCrystalActors.clear();

	if (root.contains("spawners") && root["spawners"].is_array())
	{
		for (const auto& spawnerJson : root["spawners"])
		{
			SpawnerData spawnerData;

			if (spawnerJson.contains("transform"))
			{
				const auto& transformJson = spawnerJson["transform"];

				if (transformJson.contains("position"))
				{
					spawnerData.transform.position.x = transformJson["position"].value("x", 0.0f);
					spawnerData.transform.position.y = transformJson["position"].value("y", 0.0f);
					spawnerData.transform.position.z = transformJson["position"].value("z", 0.0f);
				}

				if (transformJson.contains("rotation"))
				{
					spawnerData.transform.rotation.x = transformJson["rotation"].value("x", 0.0f);
					spawnerData.transform.rotation.y = transformJson["rotation"].value("y", 0.0f);
					spawnerData.transform.rotation.z = transformJson["rotation"].value("z", 0.0f);
					spawnerData.transform.rotation.w = transformJson["rotation"].value("w", 1.0f);
				}

				if (transformJson.contains("scale"))
				{
					spawnerData.transform.scale.x = transformJson["scale"].value("x", 1.0f);
					spawnerData.transform.scale.y = transformJson["scale"].value("y", 1.0f);
					spawnerData.transform.scale.z = transformJson["scale"].value("z", 1.0f);
				}
			}

			if (spawnerJson.contains("boxCollider"))
			{
				const auto& boxColliderJson = spawnerJson["boxCollider"];

				if (boxColliderJson.contains("localPosition"))
				{
					spawnerData.boxColliderData.localPosition.x = boxColliderJson["localPosition"].value("x", 0.0f);
					spawnerData.boxColliderData.localPosition.y = boxColliderJson["localPosition"].value("y", 0.0f);
					spawnerData.boxColliderData.localPosition.z = boxColliderJson["localPosition"].value("z", 0.0f);
				}

				if (boxColliderJson.contains("size"))
				{
					spawnerData.boxColliderData.size.x = boxColliderJson["size"].value("x", 1.0f);
					spawnerData.boxColliderData.size.y = boxColliderJson["size"].value("y", 1.0f);
					spawnerData.boxColliderData.size.z = boxColliderJson["size"].value("z", 1.0f);
				}

				spawnerData.boxColliderData.staticFriction = boxColliderJson.value("staticFriction", 0.5f);
				spawnerData.boxColliderData.dynamicFriction = boxColliderJson.value("dynamicFriction", 0.5f);
				spawnerData.boxColliderData.restitution = boxColliderJson.value("restitution", 0.0f);
			}

			if (spawnerJson.contains("spawnerType"))
			{
				std::string spawnerTypeName = spawnerJson.value("spawnerType", "");

				auto spawnerType = magic_enum::enum_cast<decltype(spawnerData.spawnerType)>(spawnerTypeName);

				if (spawnerType.has_value())
				{
					spawnerData.spawnerType = spawnerType.value();
				}
			}

			spawnerDataList.push_back(spawnerData);
		}
	}

	if (root.contains("props") && root["props"].is_array())
	{
		for (const auto& propJson : root["props"])
		{
			PropData propData;

			if (propJson.contains("transform"))
			{
				const auto& transformJson = propJson["transform"];

				if (transformJson.contains("position"))
				{
					propData.transform.position.x = transformJson["position"].value("x", 0.0f);
					propData.transform.position.y = transformJson["position"].value("y", 0.0f);
					propData.transform.position.z = transformJson["position"].value("z", 0.0f);
				}

				if (transformJson.contains("rotation"))
				{
					propData.transform.rotation.x = transformJson["rotation"].value("x", 0.0f);
					propData.transform.rotation.y = transformJson["rotation"].value("y", 0.0f);
					propData.transform.rotation.z = transformJson["rotation"].value("z", 0.0f);
					propData.transform.rotation.w = transformJson["rotation"].value("w", 1.0f);
				}

				if (transformJson.contains("scale"))
				{
					propData.transform.scale.x = transformJson["scale"].value("x", 1.0f);
					propData.transform.scale.y = transformJson["scale"].value("y", 1.0f);
					propData.transform.scale.z = transformJson["scale"].value("z", 1.0f);
				}
			}

			if (propJson.contains("colliderType"))
			{
				std::string colliderTypeName = propJson.value("colliderType", "Box");
				auto colliderType = magic_enum::enum_cast<ColliderType>(colliderTypeName);
				if (colliderType.has_value())
					propData.colliderType = colliderType.value();
			}
			else if (propJson.contains("MeshCollider"))
			{
				propData.colliderType = propJson.value("MeshCollider", false)
					? ColliderType::Mesh
					: ColliderType::Box;
			}

			if (propJson.contains("boxCollider"))
			{
				const auto& boxColliderJson = propJson["boxCollider"];

				if (boxColliderJson.contains("localPosition"))
				{
					propData.boxColliderData.localPosition.x = boxColliderJson["localPosition"].value("x", 0.0f);
					propData.boxColliderData.localPosition.y = boxColliderJson["localPosition"].value("y", 0.0f);
					propData.boxColliderData.localPosition.z = boxColliderJson["localPosition"].value("z", 0.0f);
				}

				if (boxColliderJson.contains("size"))
				{
					propData.boxColliderData.size.x = boxColliderJson["size"].value("x", 1.0f);
					propData.boxColliderData.size.y = boxColliderJson["size"].value("y", 1.0f);
					propData.boxColliderData.size.z = boxColliderJson["size"].value("z", 1.0f);
				}

				propData.boxColliderData.staticFriction = boxColliderJson.value("staticFriction", 0.5f);
				propData.boxColliderData.dynamicFriction = boxColliderJson.value("dynamicFriction", 0.5f);
				propData.boxColliderData.restitution = boxColliderJson.value("restitution", 0.0f);
			}

			if (propJson.contains("rigidbody"))
			{
				const auto& rigidbodyJson = propJson["rigidbody"];
				propData.rigidbodyData.isDynamic = rigidbodyJson.value("isDynamic", false);
			}

			if (propJson.contains("pbr"))
			{
				const auto& pbrJson = propJson["pbr"];
				if (pbrJson.contains("color"))
				{
					propData.color.x = pbrJson["color"].value("r", 1.0f);
					propData.color.y = pbrJson["color"].value("g", 1.0f);
					propData.color.z = pbrJson["color"].value("b", 1.0f);
					propData.color.w = pbrJson["color"].value("a", 1.0f);
				}
				if (pbrJson.contains("emission"))
				{
					propData.emission.x = pbrJson["emission"].value("r", propData.emission.x);
					propData.emission.y = pbrJson["emission"].value("g", propData.emission.y);
					propData.emission.z = pbrJson["emission"].value("b", propData.emission.z);
					propData.emission.w = pbrJson["emission"].value("a", propData.emission.w);
				}
				propData.metallic = pbrJson.value("metallic", propData.metallic);
				propData.roughness = pbrJson.value("roughness", propData.roughness);
				propData.occlusion = pbrJson.value("occlusion", propData.occlusion);
				propData.occlusionStrength = pbrJson.value("occlusionStrength", propData.occlusionStrength);
				propData.shadowStrength = pbrJson.value("shadowStrength", propData.shadowStrength);
				propData.isFlatShading = pbrJson.value("IsFlatShading", propData.isFlatShading);
			}

			propData.useDestroy = propJson.value("useDestroy", false);
			propData.destroyLife = propJson.value("destroyLife", 0.0f);
			if (propJson.contains("destroyableHP"))
			{
				float oldDestroyLife = static_cast<float>(propJson.value("destroyableHP", 0));
				propData.useDestroy = oldDestroyLife > 0.0f;
				propData.destroyLife = oldDestroyLife;
			}

			propData.modelPath = propJson.value("modelPath", "");

			propDataList.push_back(propData);

			auto propActor = std::make_shared<Prop>(propData);
			addedRealActors.push_back(propActor.get());
			addedPropActors.push_back(propActor.get());
			stage->GetActorManager().Register(propActor);
		}
	}

	if (root.contains("crystals") && root["crystals"].is_array())
	{
		for (const auto& crystalJson : root["crystals"])
		{
			std::vector<Transform> transforms;
			if (crystalJson.contains("transforms") && crystalJson["transforms"].is_array())
			{
				Transform parentTransform;
				if (crystalJson.contains("transform")) LoadTransformJson(crystalJson["transform"], parentTransform);
				parentTransform.Update();

				for (const auto& transformJson : crystalJson["transforms"])
				{
					Transform transform;
					LoadTransformJson(transformJson, transform);
					transform.Update();
					transforms.emplace_back(transform.matrix * parentTransform.matrix);
				}
			}
			else
			{
				Transform transform;
				if (crystalJson.contains("transform")) LoadTransformJson(crystalJson["transform"], transform);
				transforms.push_back(transform);
			}

			for (const Transform& transform : transforms)
			{
				CrystalData crystalData;
				crystalData.transform = transform;
				crystalDataList.push_back(crystalData);

				auto crystalActor = std::make_shared<CrystalProp>(crystalData);
				crystalActor->SetDestroyedCallback([this](CrystalProp* destroyedCrystal)
				{
					for (Actor*& addedActor : addedRealActors)
					{
						if (addedActor == destroyedCrystal) addedActor = nullptr;
					}
					for (CrystalProp*& addedActor : addedCrystalActors)
					{
						if (addedActor == destroyedCrystal) addedActor = nullptr;
					}
				});
				crystalActor->SetBreakParticleSystem(crystalBreakParticleSystem);
				addedRealActors.push_back(crystalActor.get());
				addedCrystalActors.push_back(crystalActor.get());
				stage->GetActorManager().Register(crystalActor);
			}
		}
	}
}

void StageLoader::LoadJsonText(const std::string& text)
{
	jsonText = text;
	jsonPath.clear();
	LoadJson();
}

std::string StageLoader::SaveJsonText()
{
	const std::filesystem::path previousPath = jsonPath;
	jsonPath.clear();
	SaveJson();
	jsonPath = previousPath;
	return jsonText;
}

void StageLoader::SaveJson()
{
	json root;

	root["spawners"] = json::array();
	root["props"] = json::array();
	root["crystals"] = json::array();

	for (const auto& spawnerData : spawnerDataList)
	{
		json spawnerJson;

		spawnerJson["transform"]["position"]["x"] = spawnerData.transform.position.x;
		spawnerJson["transform"]["position"]["y"] = spawnerData.transform.position.y;
		spawnerJson["transform"]["position"]["z"] = spawnerData.transform.position.z;

		spawnerJson["transform"]["rotation"]["x"] = spawnerData.transform.rotation.x;
		spawnerJson["transform"]["rotation"]["y"] = spawnerData.transform.rotation.y;
		spawnerJson["transform"]["rotation"]["z"] = spawnerData.transform.rotation.z;
		spawnerJson["transform"]["rotation"]["w"] = spawnerData.transform.rotation.w;

		spawnerJson["transform"]["scale"]["x"] = spawnerData.transform.scale.x;
		spawnerJson["transform"]["scale"]["y"] = spawnerData.transform.scale.y;
		spawnerJson["transform"]["scale"]["z"] = spawnerData.transform.scale.z;

		spawnerJson["boxCollider"]["localPosition"]["x"] = spawnerData.boxColliderData.localPosition.x;
		spawnerJson["boxCollider"]["localPosition"]["y"] = spawnerData.boxColliderData.localPosition.y;
		spawnerJson["boxCollider"]["localPosition"]["z"] = spawnerData.boxColliderData.localPosition.z;

		spawnerJson["boxCollider"]["size"]["x"] = spawnerData.boxColliderData.size.x;
		spawnerJson["boxCollider"]["size"]["y"] = spawnerData.boxColliderData.size.y;
		spawnerJson["boxCollider"]["size"]["z"] = spawnerData.boxColliderData.size.z;

		spawnerJson["boxCollider"]["staticFriction"] = spawnerData.boxColliderData.staticFriction;
		spawnerJson["boxCollider"]["dynamicFriction"] = spawnerData.boxColliderData.dynamicFriction;
		spawnerJson["boxCollider"]["restitution"] = spawnerData.boxColliderData.restitution;

		spawnerJson["spawnerType"] = std::string(magic_enum::enum_name(spawnerData.spawnerType));

		root["spawners"].push_back(spawnerJson);
	}

	for (const auto& propData : propDataList)
	{
		json propJson;

		propJson["transform"]["position"]["x"] = propData.transform.position.x;
		propJson["transform"]["position"]["y"] = propData.transform.position.y;
		propJson["transform"]["position"]["z"] = propData.transform.position.z;

		propJson["transform"]["rotation"]["x"] = propData.transform.rotation.x;
		propJson["transform"]["rotation"]["y"] = propData.transform.rotation.y;
		propJson["transform"]["rotation"]["z"] = propData.transform.rotation.z;
		propJson["transform"]["rotation"]["w"] = propData.transform.rotation.w;

		propJson["transform"]["scale"]["x"] = propData.transform.scale.x;
		propJson["transform"]["scale"]["y"] = propData.transform.scale.y;
		propJson["transform"]["scale"]["z"] = propData.transform.scale.z;

		propJson["boxCollider"]["localPosition"]["x"] = propData.boxColliderData.localPosition.x;
		propJson["boxCollider"]["localPosition"]["y"] = propData.boxColliderData.localPosition.y;
		propJson["boxCollider"]["localPosition"]["z"] = propData.boxColliderData.localPosition.z;

		propJson["boxCollider"]["size"]["x"] = propData.boxColliderData.size.x;
		propJson["boxCollider"]["size"]["y"] = propData.boxColliderData.size.y;
		propJson["boxCollider"]["size"]["z"] = propData.boxColliderData.size.z;

		propJson["boxCollider"]["staticFriction"] = propData.boxColliderData.staticFriction;
		propJson["boxCollider"]["dynamicFriction"] = propData.boxColliderData.dynamicFriction;
		propJson["boxCollider"]["restitution"] = propData.boxColliderData.restitution;

		propJson["rigidbody"]["isDynamic"] = propData.rigidbodyData.isDynamic;
		propJson["colliderType"] = std::string(magic_enum::enum_name(propData.colliderType));

		propJson["pbr"]["color"]["r"] = propData.color.x;
		propJson["pbr"]["color"]["g"] = propData.color.y;
		propJson["pbr"]["color"]["b"] = propData.color.z;
		propJson["pbr"]["color"]["a"] = propData.color.w;
		propJson["pbr"]["emission"]["r"] = propData.emission.x;
		propJson["pbr"]["emission"]["g"] = propData.emission.y;
		propJson["pbr"]["emission"]["b"] = propData.emission.z;
		propJson["pbr"]["emission"]["a"] = propData.emission.w;
		propJson["pbr"]["metallic"] = propData.metallic;
		propJson["pbr"]["roughness"] = propData.roughness;
		propJson["pbr"]["occlusion"] = propData.occlusion;
		propJson["pbr"]["occlusionStrength"] = propData.occlusionStrength;
		propJson["pbr"]["shadowStrength"] = propData.shadowStrength;
		propJson["pbr"]["IsFlatShading"] = propData.isFlatShading;

		propJson["useDestroy"] = propData.useDestroy;
		propJson["destroyLife"] = propData.destroyLife;

		propJson["modelPath"] = propData.modelPath;

		root["props"].push_back(propJson);
	}

	for (const CrystalData& crystalData : crystalDataList)
	{
		json crystalJson;
		SaveTransformJson(crystalJson["transform"], crystalData.transform);
		root["crystals"].push_back(crystalJson);
	}
	jsonText = root.dump(4);
	if (jsonPath.empty()) return;
	if (jsonPath.has_parent_path())
	{
		std::filesystem::create_directories(jsonPath.parent_path());
	}

	std::ofstream ofs(jsonPath);

	if (!ofs)
	{
		return;
	}

	ofs << jsonText;
}


























