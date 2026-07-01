// StageLoader.cpp

#include "StageLoader.h"
#include "Graphics.h"
#include "magic_enum/magic_enum.hpp"
#include <fstream>
#include <iomanip>
#include "ResourceManager.h"
#include "GameTime.h"
#include "ActorManager.h"
#include "Prop.h"

using json = nlohmann::json;

StageLoader::StageLoader(Object* owner, Actor* stage, std::filesystem::path jsonPath)
	: Component(owner), stage(stage), jsonPath(jsonPath)
{
	LoadJson();
}

void StageLoader::Update()
{
	for (auto& prop : propDataList)
	{
		if (!prop.model)
		{
			prop.model = ResourceManager::Instance().LoadModel(prop.modelPath);
		}

		float t = Game::Time::time;

		float cycle = std::fmod(t, 1.0f);

		float alpha;
		if (cycle < 0.5f)
		{
			alpha = cycle / 0.5f;
		}
		else
		{
			alpha = 1.0f - ((cycle - 0.5f) / 0.5f);
		}

		ShaderParamList param;
		param.push_back({"metalness", prop.metallic});
		param.push_back({"roughness", prop.roughness});
		param.push_back({"occlusion", prop.occlusion});
		param.push_back({"occlusionStrength", prop.occlusionStrength});
		param.push_back({"color", Color(1.0f, 1.0f, 1.0f, alpha)});
		ModelRenderer::SetShaderParamForAllMaterials(prop.model.get(), param, prop.shaderParams);

		prop.transform.Update();
		prop.model->UpdateTransform(prop.transform.matrix);
	}
}

void StageLoader::Render(const RenderContext& rc)
{
	if (!showDebug) return;

	for (auto& prop : propDataList)
	{
		// モデル描画
		Game::Graphics::Instance().GetModelRenderer()->Draw(ModelShaderId::PBR, prop.model, prop.shaderParams);
	}

	// 指定秒間隔で点滅
	for (auto& spawner : spawnerDataList)
	{
		// スポーン範囲描画
		Matrix world = spawner.transform.matrix * Matrix::CreateTranslation(spawner.boxColliderData.localPosition);
		Game::Graphics::Instance().GetShapeRenderer()->DrawBox(
			world.Translation(), spawner.transform.rotation.ToEuler(), spawner.boxColliderData.size, {1, 1, 1});
	}
	for (auto& prop : propDataList)
	{
		// 当たり判定描画
		Matrix world = prop.transform.matrix * Matrix::CreateTranslation(prop.boxColliderData.localPosition);
		Game::Graphics::Instance().GetShapeRenderer()->DrawBox(
			world.Translation(), prop.transform.rotation.ToEuler(), prop.boxColliderData.size, {1, 1, 1});
	}
}

void StageLoader::DrawGUI()
{
	ImGui::Text("Json Path: %s", jsonPath.string().c_str());

	{
		float availWidth = ImGui::GetContentRegionAvail().x;
		float spacing = ImGui::GetStyle().ItemSpacing.x;
		float buttonWidth = (availWidth - spacing) * 0.5f;

		// セーブ

		if (ImGui::Button("Save", ImVec2(buttonWidth, 0.0f)))
		{
			SaveJson();
		}

		ImGui::SameLine();

		// リロード

		if (ImGui::Button("Reload", ImVec2(buttonWidth, 0.0f)))
		{
			LoadJson();
		}

		ImGui::Separator();
	}

	// 追加

	ImGui::Text("Add Object:");
	if (ImGui::BeginChild("##addwin", ImVec2(0.0f, 200.0f), true))
	{
		// 追加タイプ

		using AddType = decltype(addType);
		std::string previewName = std::string(magic_enum::enum_name(addType));
		if (ImGui::BeginCombo("##StageLoaderCombo", previewName.c_str()))
		{
			for (AddType type : magic_enum::enum_values<AddType>())
			{
				std::string name = std::string(magic_enum::enum_name(type));
				bool isSelected = (addType == type);

				if (ImGui::Selectable(name.c_str(), isSelected))
				{
					addType = type;
				}

				if (isSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}

			ImGui::EndCombo();
		}

		// 追加詳細

		if (addType == AddType::Spawner)
		{
			// transform
			addSpawnerData.transform.DrawGUI();

			// boxCollider
			addSpawnerData.boxColliderData.DrawGUI();

			// スポーナータイプ
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

			// 追加ボタン

			if (ImGui::Button((const char*)u8"Add to loader", ImVec2(-FLT_MIN, 30.0f)))
			{
				spawnerDataList.push_back(addSpawnerData);
			}

		}
		else if (addType == AddType::Prop)
		{
			// transform
			addPropData.transform.DrawGUI();

			// boxCollider
			addPropData.boxColliderData.DrawGUI();

			// rigidbody
			addPropData.rigidbodyData.DrawGUI();

			// プロップの場合はパスが詳細。パスの最初に#があればダイナミック

			std::vector<std::filesystem::path> glbFiles;
			for (const auto& file : std::filesystem::directory_iterator("Data/Model/Prop"))
			{
				if (file.path().extension() == ".glb")
				{
					glbFiles.push_back(file.path());
				}
			}

			// モデルパス

			ImGui::Text("Model Path:");
			if (ImGui::BeginCombo("##StageLoaderPropDetail", std::filesystem::path(addPropData.modelPath).filename().string().c_str()))
			{
				for (const auto& path : glbFiles)
				{
					std::string name = path.filename().string();
					bool isSelected = (addPropData.modelPath == name);
					if (ImGui::Selectable(name.c_str(), isSelected))
					{
						addPropData.modelPath = "Data/Model/Prop/" + name;
					}
					if (isSelected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			// 追加ボタン

			if (ImGui::Button((const char*)u8"Add to loader", ImVec2(-FLT_MIN, 30.0f)))
			{
				if (std::filesystem::exists(addPropData.modelPath))
				{
					propDataList.push_back(addPropData);
				}
			}
		}
	}

	ImGui::EndChild();

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

	ImGui::TextUnformatted("PropList:");

	ImGui::BeginChild("##proplistwin", ImVec2(0.0f, 150.0f), true);

	for (int i = 0; i < static_cast<int>(propDataList.size()); ++i)
	{
		auto& propData = propDataList[i];

		ImGui::PushID(i);

		if (ImGui::TreeNode("Prop"))
		{
			propData.transform.DrawGUI();
			propData.boxColliderData.DrawGUI();
			propData.rigidbodyData.DrawGUI();

			ImGui::Text("Model Path: %s", propData.modelPath.c_str());

			if (ImGui::Button("Remove"))
			{
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

void StageLoader::LoadJson()
{
	if (!std::filesystem::exists(jsonPath))
	{
		return;
	}

	std::ifstream ifs(jsonPath);

	if (!ifs)
	{
		return;
	}

	json root;

	try
	{
		ifs >> root;
	}
	catch (const json::parse_error&)
	{
		return;
	}

	spawnerDataList.clear();
	propDataList.clear();

	for (auto addedActor : addedRealActors)
	{
		addedActor->Destroy();
	}

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

			propData.modelPath = propJson.value("modelPath", "");

			propDataList.push_back(propData);

			// リアルにステージに配置
			stage->GetActorManager()->Register(std::make_shared<Prop>(propData));
		}
	}
}

void StageLoader::SaveJson()
{
	json root;

	root["spawners"] = json::array();
	root["props"] = json::array();

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

		propJson["modelPath"] = propData.modelPath;

		root["props"].push_back(propJson);
	}

	if (jsonPath.has_parent_path())
	{
		std::filesystem::create_directories(jsonPath.parent_path());
	}

	std::ofstream ofs(jsonPath);

	if (!ofs)
	{
		return;
	}

	ofs << std::setw(4) << root;
}
