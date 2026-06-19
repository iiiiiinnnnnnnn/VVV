// StageLoader.cpp

#include "StageLoader.h"

#include "ActorManager.h"
#include "Aracore.h"
#include "AracoreQueen.h"
#include "Prop.h"
#include "Components.h"

#include <fstream>

using json = nlohmann::json;

StageLoader::StageLoader(
	Object* owner,
	Actor* stage,
	std::filesystem::path jsonPath)
	: Component(owner)
	, stage(stage)
	, jsonPath(jsonPath)
{
}

void StageLoader::Update()
{
	if (loaded)
	{
		return;
	}

	if (!stage)
	{
		return;
	}

	if (!stage->GetActorManager())
	{
		return;
	}

	LoadStageFromJSON();
	loaded = true;
}

void StageLoader::LoadStageFromJSON()
{
	ClearLoadedActors();

	if (std::filesystem::exists(jsonPath))
	{
		std::ifstream ifs(jsonPath);

		if (ifs)
		{
			try
			{
				ifs >> stageJson;
			}
			catch (const std::exception& e)
			{
				printf("Stage JSON parse failed: %s\n", e.what());
				stageJson = json::object();
			}
		}
	}
	else
	{
		stageJson = json::object();
	}

	if (!stageJson.contains("props") || !stageJson["props"].is_array())
	{
		stageJson["props"] = json::array();
	}

	if (!stageJson.contains("enemies") || !stageJson["enemies"].is_array())
	{
		stageJson["enemies"] = json::array();
	}

	if (!stageJson.contains("bosses") || !stageJson["bosses"].is_array())
	{
		stageJson["bosses"] = json::array();
	}

	for (const json& propJson : stageJson["props"])
	{
		SpawnActorFromJSON(propJson, "props");
	}

	for (const json& enemyJson : stageJson["enemies"])
	{
		SpawnActorFromJSON(enemyJson, "enemies");
	}

	for (const json& bossJson : stageJson["bosses"])
	{
		SpawnActorFromJSON(bossJson, "bosses");
	}
}

void StageLoader::SaveStageToJSON()
{
	stageJson =
		BuildStageJSONFromLoadedActors();

	std::filesystem::path parentPath =
		jsonPath.parent_path();

	if (!parentPath.empty())
	{
		std::filesystem::create_directories(parentPath);
	}

	std::ofstream ofs(jsonPath);

	if (!ofs)
	{
		printf(
			"Stage JSON save failed: %s\n",
			jsonPath.string().c_str());

		return;
	}

	ofs << stageJson.dump(4);
}

void StageLoader::ClearLoadedActors()
{
	for (LoadedActor& loadedActor : loadedActors)
	{
		if (loadedActor.actor)
		{
			loadedActor.actor->Destroy();
		}
	}

	loadedActors.clear();
	namedActors.clear();
}

Actor* StageLoader::SpawnActorFromJSON(
	const json& objectJson,
	const std::string& groupName)
{
	if (!stage)
	{
		return nullptr;
	}

	ActorManager* actorManager =
		stage->GetActorManager();

	if (!actorManager)
	{
		return nullptr;
	}

	std::shared_ptr<Actor> actor;

	if (groupName == "props")
	{
		std::string path =
			objectJson.value("path", std::string());

		if (path.empty())
		{
			return nullptr;
		}

		Transform transform =
			ReadTransform(objectJson);

		bool isDynamic =
			objectJson.value("isDynamic", false);

		int meshColliderConvex =
			objectJson.value("meshColliderConvex", 1280);

		int animationIndex =
			objectJson.value("animationIndex", -1);

		std::string tag =
			objectJson.value("tag", std::string("Prop"));

		bool active =
			objectJson.value("active", true);

		int layer =
			objectJson.value("layer", Layer::Default);

		actor =
			std::make_shared<Prop>(
			path,
			transform,
			isDynamic,
			meshColliderConvex,
			animationIndex,
			tag,
			active,
			layer);
	}
	else
	{
		std::string type =
			objectJson.value("type", std::string());

		if (type == "Aracore")
		{
			actor =
				std::make_shared<Aracore>();
		}
		else if (type == "AracoreQueen")
		{
			actor =
				std::make_shared<AracoreQueen>();
		}
		else
		{
			printf(
				"Unknown actor type: %s\n",
				type.c_str());

			return nullptr;
		}
	}

	if (!actor)
	{
		return nullptr;
	}

	Actor* rawActor =
		actor.get();

	actorManager->Register(actor);

	if (groupName != "props")
	{
		Transform transform =
			ReadTransform(objectJson);

		ApplyTransformToActor(rawActor, transform);
	}

	ApplyCommonActorSettings(rawActor, objectJson);

	LoadedActor loadedActor;
	loadedActor.actor = actor;
	loadedActor.groupName = groupName;
	loadedActor.objectJson = objectJson;

	loadedActors.push_back(loadedActor);

	std::string id =
		objectJson.value("id", std::string());

	if (!id.empty())
	{
		namedActors[id] = rawActor;
	}

	return rawActor;
}

json StageLoader::BuildStageJSONFromLoadedActors() const
{
	json root;

	root["props"] = json::array();
	root["enemies"] = json::array();
	root["bosses"] = json::array();

	for (const LoadedActor& loadedActor : loadedActors)
	{
		if (!loadedActor.actor)
		{
			continue;
		}

		if (loadedActor.actor->IsPendingDestroy())
		{
			continue;
		}

		json objectJson =
			BuildObjectJSONFromLoadedActor(loadedActor);

		if (loadedActor.groupName == "props")
		{
			root["props"].push_back(objectJson);
		}
		else if (loadedActor.groupName == "enemies")
		{
			root["enemies"].push_back(objectJson);
		}
		else if (loadedActor.groupName == "bosses")
		{
			root["bosses"].push_back(objectJson);
		}
	}

	return root;
}

json StageLoader::BuildObjectJSONFromLoadedActor(
	const LoadedActor& loadedActor) const
{
	json objectJson =
		loadedActor.objectJson;

	Actor* actor =
		loadedActor.actor.get();

	if (!actor)
	{
		return objectJson;
	}

	if (objectJson.contains("name"))
	{
		objectJson["name"] =
			actor->GetName();
	}
	else
	{
		objectJson["id"] =
			actor->GetName();
	}

	objectJson["tag"] =
		actor->GetTag();

	objectJson["layer"] =
		actor->GetLayer();

	objectJson["active"] =
		actor->IsActive();

	WriteTransformToJSON(
		objectJson,
		actor->transform);

	return objectJson;
}

void StageLoader::WriteTransformToJSON(
	json& objectJson,
	const Transform& transform) const
{
	objectJson["position"] =
	{
		transform.position.x,
		transform.position.y,
		transform.position.z
	};

	objectJson["rotation"] =
	{
		transform.rotation.x,
		transform.rotation.y,
		transform.rotation.z,
		transform.rotation.w
	};

	objectJson["scale"] =
	{
		transform.scale.x,
		transform.scale.y,
		transform.scale.z
	};
}

Transform StageLoader::ReadTransform(
	const json& objectJson) const
{
	Transform transform;

	transform.position =
		ReadVector3(
		objectJson,
		"position",
		Vector3::Zero);

	transform.scale =
		ReadVector3(
		objectJson,
		"scale",
		Vector3::One);

	transform.rotation =
		Quaternion::Identity;

	if (objectJson.contains("rotation") &&
		objectJson["rotation"].is_array())
	{
		const json& rotation =
			objectJson["rotation"];

		if (rotation.size() >= 4)
		{
			transform.rotation =
				Quaternion(
				rotation[0].get<float>(),
				rotation[1].get<float>(),
				rotation[2].get<float>(),
				rotation[3].get<float>());
		}
		else if (rotation.size() >= 3)
		{
			Vector3 euler =
				Vector3(
				rotation[0].get<float>(),
				rotation[1].get<float>(),
				rotation[2].get<float>());

			transform.rotation =
				Quaternion::CreateFromYawPitchRoll(
				RAD(euler.y),
				RAD(euler.x),
				RAD(euler.z));
		}
	}

	transform.Update();

	return transform;
}

Vector3 StageLoader::ReadVector3(
	const json& objectJson,
	const char* key,
	const Vector3& defaultValue) const
{
	if (!objectJson.contains(key))
	{
		return defaultValue;
	}

	const json& value =
		objectJson[key];

	if (!value.is_array() ||
		value.size() < 3)
	{
		return defaultValue;
	}

	return Vector3(
		value[0].get<float>(),
		value[1].get<float>(),
		value[2].get<float>());
}

void StageLoader::ApplyTransformToActor(
	Actor* actor,
	const Transform& transform)
{
	if (!actor)
	{
		return;
	}

	actor->transform = transform;
	actor->transform.Update();

	Rigidbody* rb =
		actor->GetComponent<Rigidbody>();

	if (rb)
	{
		rb->SetPosition(
			actor->transform.position);

		rb->SetRotation(
			actor->transform.rotation);

		if (RigidbodyDynamic* dynamicRb =
			dynamic_cast<RigidbodyDynamic*>(rb))
		{
			dynamicRb->SetVelocity(
				Vector3::Zero);
		}

		return;
	}

	CharacterController* characterController =
		actor->GetComponent<CharacterController>();

	if (characterController)
	{
		characterController->SetFootPosition(
			actor->transform.position);
	}
}

void StageLoader::ApplyCommonActorSettings(
	Actor* actor,
	const json& objectJson)
{
	if (!actor)
	{
		return;
	}

	if (objectJson.contains("name"))
	{
		actor->SetName(
			objectJson["name"].get<std::string>());
	}
	else if (objectJson.contains("id"))
	{
		actor->SetName(
			objectJson["id"].get<std::string>());
	}

	if (objectJson.contains("tag"))
	{
		actor->SetTag(
			objectJson["tag"].get<std::string>());
	}

	if (objectJson.contains("layer"))
	{
		actor->SetLayer(
			objectJson["layer"].get<int>());
	}

	if (objectJson.contains("active"))
	{
		actor->SetActive(
			objectJson["active"].get<bool>());
	}
}

json StageLoader::MakeAddObjectJSON() const
{
	json objectJson;

	objectJson["id"] =
		addId;

	objectJson["position"] =
	{
		addPosition.x,
		addPosition.y,
		addPosition.z
	};

	objectJson["rotation"] =
	{
		addRotation.x,
		addRotation.y,
		addRotation.z
	};

	objectJson["scale"] =
	{
		addScale.x,
		addScale.y,
		addScale.z
	};

	if (addTypeIndex == 0)
	{
		objectJson["path"] =
			addPropPath;

		objectJson["isDynamic"] =
			addIsDynamic;

		objectJson["meshColliderConvex"] =
			addMeshColliderConvex;

		objectJson["tag"] =
			"Prop";

		objectJson["layer"] =
			Layer::Default;
	}
	else if (addTypeIndex == 1)
	{
		objectJson["type"] =
			"Aracore";

		objectJson["aggressive"] =
			addAggressive;

		objectJson["tag"] =
			"Enemy";

		objectJson["layer"] =
			Layer::Enemy;
	}
	else if (addTypeIndex == 2)
	{
		objectJson["type"] =
			"AracoreQueen";

		objectJson["aggressive"] =
			addAggressive;

		objectJson["tag"] =
			"Enemy";

		objectJson["layer"] =
			Layer::Enemy;
	}

	return objectJson;
}

void StageLoader::DrawGUI()
{
	if (ImGui::TreeNode("StageLoader"))
	{
		ImGui::Text(
			"Json: %s",
			jsonPath.string().c_str());

		ImGui::Text(
			"Loaded Actors: %d",
			(int)loadedActors.size());

		if (ImGui::Button("Reload"))
		{
			LoadStageFromJSON();
			loaded = true;
		}

		ImGui::SameLine();

		if (ImGui::Button("Save"))
		{
			SaveStageToJSON();
		}

		DrawAddObjectGUI();
		DrawLoadedActorsGUI();

		ImGui::TreePop();
	}
}

void StageLoader::DrawAddObjectGUI()
{
	if (!ImGui::TreeNode("Add Object"))
	{
		return;
	}

	const char* typeItems[] =
	{
		"Prop",
		"Aracore",
		"AracoreQueen"
	};

	ImGui::Combo(
		"Type",
		&addTypeIndex,
		typeItems,
		_countof(typeItems));

	ImGui::InputText(
		"Id",
		&addId);

	if (addTypeIndex == 0)
	{
		ImGui::InputText(
			"Path",
			&addPropPath);

		ImGui::Checkbox(
			"Is Dynamic",
			&addIsDynamic);

		ImGui::DragInt(
			"Mesh Collider Convex",
			&addMeshColliderConvex,
			1.0f,
			0,
			4096);
	}

	ImGui::DragFloat3(
		"Position",
		&addPosition.x,
		0.1f);

	ImGui::DragFloat3(
		"Rotation",
		&addRotation.x,
		0.1f);

	ImGui::DragFloat3(
		"Scale",
		&addScale.x,
		0.01f);

	if (ImGui::Button("Add"))
	{
		json objectJson =
			MakeAddObjectJSON();

		if (addTypeIndex == 0)
		{
			SpawnActorFromJSON(
				objectJson,
				"props");
		}
		else if (addTypeIndex == 1)
		{
			SpawnActorFromJSON(
				objectJson,
				"enemies");
		}
		else if (addTypeIndex == 2)
		{
			SpawnActorFromJSON(
				objectJson,
				"bosses");
		}
	}
	ImGui::TreePop();
}

void StageLoader::DrawLoadedActorsGUI()
{
	if (!ImGui::TreeNode("Loaded Actors"))
	{
		return;
	}

	for (size_t i = 0; i < loadedActors.size();)
	{
		LoadedActor& loadedActor =
			loadedActors[i];

		Actor* actor =
			loadedActor.actor.get();

		if (!actor)
		{
			loadedActors.erase(
				loadedActors.begin() + i);

			continue;
		}

		if (actor->IsPendingDestroy())
		{
			++i;
			continue;
		}

		ImGui::PushID(actor);

		const std::string& name =
			actor->GetName();

		if (ImGui::TreeNode(
			name.empty() ? "Actor" : name.c_str()))
		{
			ImGui::Text(
				"Group: %s",
				loadedActor.groupName.c_str());

			actor->DrawGUI();

			if (ImGui::Button("Remove From Stage"))
			{
				for (auto it = namedActors.begin();
					it != namedActors.end();)
				{
					if (it->second == actor)
					{
						it =
							namedActors.erase(it);
					}
					else
					{
						++it;
					}
				}

				actor->Destroy();

				loadedActors.erase(
					loadedActors.begin() + i);

				ImGui::TreePop();
				ImGui::PopID();
				continue;
			}

			ImGui::TreePop();
		}

		ImGui::PopID();

		++i;
	}

	ImGui::TreePop();
}
