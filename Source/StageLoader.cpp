// StageLoader.cpp

#include "StageLoader.h"

#include "ActorManager.h"
#include "Aracore.h"
#include "AracoreQueen.h"
#include "Components.h"
#include "NavMeshActor.h"
#include "NavMeshObstacle.h"
#include "PhysicsManager.h"

#include <fstream>
#include <cfloat>
#include "ResourceManager.h"
#include "IconsFontAwesome5.h"

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
	if (reloadRequested)
	{
		reloadRequested = false;
		LoadStageFromJSON();
		loaded = true;
		return;
	}

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

	if (NavMeshActor* navMeshActor = NavMeshActor::GetActive())
		navMeshActor->RequestBuild(1);
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
	PhysicsManager::Instance().GetSceneContext().ClearCollisionEvents();

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
	bool isNavmeshObject = false;

	std::string id = objectJson.value("id", std::string());

	if (groupName == "props")
	{
		std::string path = objectJson.value("path", std::string());

		if (path.empty())
			return nullptr;

		Transform transform =
			ReadTransform(objectJson, true);

		bool isDynamic =
			objectJson.value("isDynamic", false);

		int animationIndex =
			objectJson.value("animationIndex", -1);

		std::string tag =
			objectJson.value("tag", std::string("Prop"));

		bool active =
			objectJson.value("active", true);

		int layer =
			objectJson.value("layer", Layer::Default);

		isNavmeshObject =
			ReadNavmeshObjectFlag(objectJson);

		actor = std::make_shared<Actor>(id, tag, active, layer);
		actor->transform = transform;
		auto model = ResourceManager::Instance().LoadModel(path);
		actor->AddComponent<ModelRenderComponent>(model, ModelShaderId::PBR);
		if (animationIndex != -1)
		{
			Animator* anim = actor->AddComponent<Animator>(model, animationIndex);
			anim->Play(0, animationIndex, true);
		}
		Rigidbody* rb = nullptr;
		if (isDynamic)
		{
			rb = actor->AddComponent<RigidbodyDynamic>();
		}
		else
		{
			rb = actor->AddComponent<RigidbodyStatic>();
		}
		AddPropCollider(
			actor.get(),
			rb,
			model.get(),
			objectJson,
			transform);

		ApplyNavmeshObjectToProp(
			actor.get(),
			isNavmeshObject);
	}
	else
	{
		std::string type =
			objectJson.value("type", std::string());

		if (type == "Aracore")
		{
			actor = std::make_shared<Aracore>();
		}
		else if (type == "AracoreQueen")
		{
			actor = std::make_shared<AracoreQueen>();
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
		return nullptr;

	Actor* rawActor = actor.get();
	actorManager->Register(actor);

	Transform transform =
		ReadTransform(objectJson, groupName == "props");

	ApplyTransformToActor(rawActor, transform, groupName != "props");

	ApplyCommonActorSettings(rawActor, objectJson);

	if (groupName == "props")
	{
		ApplyNavmeshObjectToProp(
			rawActor,
			isNavmeshObject);
	}

	LoadedActor loadedActor;
	loadedActor.actor = actor;
	loadedActor.groupName = groupName;
	loadedActor.objectJson = objectJson;
	loadedActor.isNavmeshObject = isNavmeshObject;

	if (groupName == "props")
	{
		loadedActor.objectJson["isNavmeshObject"] =
			loadedActor.isNavmeshObject;
	}

	loadedActors.push_back(loadedActor);

	if (!id.empty())
		namedActors[id] = rawActor;

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

	if (loadedActor.groupName == "props")
	{
		WriteNavmeshObjectFlag(
			objectJson,
			loadedActor.isNavmeshObject);

		WritePropColliderToJSON(objectJson, actor);
	}
	else
	{
		objectJson.erase("isNavmeshObject");
	}

	WriteTransformToJSON(
		objectJson,
		actor->transform,
		loadedActor.groupName == "props");

	return objectJson;
}

bool StageLoader::ReadNavmeshObjectFlag(
	const json& objectJson) const
{
	const char* key = nullptr;
	if (objectJson.contains("isNavmeshObject"))
	{
		key = "isNavmeshObject";
	}
	else if (objectJson.contains("isNavMeshObject"))
	{
		key = "isNavMeshObject";
	}
	else
	{
		return false;
	}

	const json& value =
		objectJson[key];

	if (value.is_boolean())
	{
		return value.get<bool>();
	}

	if (value.is_number_integer())
	{
		return value.get<int>() != 0;
	}

	if (value.is_string())
	{
		const std::string text =
			value.get<std::string>();

		return text == "true" ||
			text == "True" ||
			text == "TRUE" ||
			text == "1";
	}

	return false;
}

void StageLoader::WriteNavmeshObjectFlag(
	json& objectJson,
	bool isNavmeshObject) const
{
	objectJson.erase("isNavMeshObject");
	objectJson["isNavmeshObject"] =
		isNavmeshObject;
}

void StageLoader::ApplyNavmeshObjectToProp(
	Actor* actor,
	bool isNavmeshObject)
{
	if (!actor)
	{
		return;
	}

	if (!isNavmeshObject)
	{
		return;
	}

	if (actor->GetComponent<NavMeshObstacle>())
	{
		return;
	}

	actor->AddComponent<NavMeshObstacle>();
}

void StageLoader::WriteTransformToJSON(
	json& objectJson,
	const Transform& transform,
	bool writeScale) const
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

	if (writeScale)
	{
		objectJson["scale"] =
		{
			transform.scale.x,
			transform.scale.y,
			transform.scale.z
		};
	}
	else
	{
		objectJson.erase("scale");
	}
}

void StageLoader::WritePropColliderToJSON(
	json& objectJson,
	Actor* actor) const
{
	objectJson.erase("colliderType");
	objectJson.erase("meshColliderConvex");

	if (!actor)
		return;

	BoxCollider* boxCollider =
		actor->GetComponent<BoxCollider>();

	if (!boxCollider)
		return;

	const Vector3& size =
		boxCollider->GetSize();

	const Vector3& offset =
		boxCollider->GetLocalPosition();

	objectJson["boxColliderSize"] =
	{
		size.x,
		size.y,
		size.z
	};

	objectJson["boxColliderOffset"] =
	{
		offset.x,
		offset.y,
		offset.z
	};
}

Transform StageLoader::ReadTransform(
	const json& objectJson,
	bool readScale) const
{
	Transform transform;

	transform.position =
		ReadVector3(
		objectJson,
		"position",
		Vector3::Zero);

	transform.scale = readScale
		? ReadVector3(
			objectJson,
			"scale",
			Vector3::One)
		: Vector3::One;

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
		return defaultValue;

	const json& value =
		objectJson[key];

	if (!value.is_array() || value.size() < 3)
		return defaultValue;

	return Vector3(
		value[0].get<float>(),
		value[1].get<float>(),
		value[2].get<float>());
}

void StageLoader::AddPropCollider(
	Actor* actor,
	Rigidbody* rb,
	Model* model,
	const json& objectJson,
	const Transform& transform)
{
	if (!actor || !rb)
		return;

	Vector3 defaultCenter;
	Vector3 defaultSize;
	GetModelLocalBounds(model, defaultCenter, defaultSize);

	defaultCenter.x *= transform.scale.x;
	defaultCenter.y *= transform.scale.y;
	defaultCenter.z *= transform.scale.z;

	defaultSize.x *= fabsf(transform.scale.x);
	defaultSize.y *= fabsf(transform.scale.y);
	defaultSize.z *= fabsf(transform.scale.z);

	Vector3 boxColliderSize =
		ReadVector3(
			objectJson,
			"boxColliderSize",
			defaultSize);

	Vector3 boxColliderOffset =
		ReadVector3(
			objectJson,
			"boxColliderOffset",
			defaultCenter);

	if (boxColliderSize.x <= eps ||
		boxColliderSize.y <= eps ||
		boxColliderSize.z <= eps)
	{
		return;
	}

	actor->AddComponent<BoxCollider>(
		rb,
		boxColliderSize,
		boxColliderOffset);
}

void StageLoader::GetModelLocalBounds(
	Model* model,
	Vector3& center,
	Vector3& size) const
{
	center = Vector3::Zero;
	size = Vector3::One;

	if (!model)
		return;

	Vector3 minPosition(FLT_MAX, FLT_MAX, FLT_MAX);
	Vector3 maxPosition(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	bool hasVertex = false;

	for (const Model::Mesh& mesh : model->GetMeshes())
	{
		if (!mesh.isDraw || !mesh.node)
			continue;

		for (const Model::Vertex& vertex : mesh.vertices)
		{
			Vector3 position =
				Vector3::Transform(
					vertex.position,
					mesh.node->worldTransform);

			if (position.x < minPosition.x) minPosition.x = position.x;
			if (position.y < minPosition.y) minPosition.y = position.y;
			if (position.z < minPosition.z) minPosition.z = position.z;

			if (position.x > maxPosition.x) maxPosition.x = position.x;
			if (position.y > maxPosition.y) maxPosition.y = position.y;
			if (position.z > maxPosition.z) maxPosition.z = position.z;

			hasVertex = true;
		}
	}

	if (!hasVertex)
		return;

	center =
		(minPosition + maxPosition) * 0.5f;

	size =
		maxPosition - minPosition;
}

void StageLoader::ApplyTransformToActor(
	Actor* actor,
	const Transform& transform,
	bool dontScale)
{
	if (!actor)
		return;

	actor->transform.SetPosition(transform.position);
	actor->transform.SetRotation(transform.rotation);
	if(!dontScale)
		actor->transform.SetScale(transform.scale);

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
	}

	CharacterController* characterController =
		actor->GetComponent<CharacterController>();

	if (characterController)
	{
		characterController->SetFootPosition(actor->transform.position);
	}
}

void StageLoader::ApplyCommonActorSettings(
	Actor* actor,
	const json& objectJson)
{
	if (!actor) return;

	if (objectJson.contains("name"))
	{
		actor->SetName(objectJson["name"].get<std::string>());
	}
	else if (objectJson.contains("id"))
	{
		actor->SetName(objectJson["id"].get<std::string>());
	}

	if (objectJson.contains("tag"))
	{
		actor->SetTag(objectJson["tag"].get<std::string>());
	}

	if (objectJson.contains("layer"))
	{
		actor->SetLayer(objectJson["layer"].get<int>());
	}

	if (objectJson.contains("active"))
	{
		actor->SetActive(objectJson["active"].get<bool>());
	}
}

json StageLoader::MakeAddObjectJSON() const
{
	json objectJson;
	objectJson["id"] = addId;
	objectJson["position"] = {0, 0, 0};
	objectJson["rotation"] = {0, 0, 0, 1};
	if (addTypeIndex == 0)
	{
		objectJson["scale"] = {1, 1, 1};
		objectJson["path"] = addPropPath;
		objectJson["isDynamic"] = addIsDynamic;
		objectJson["isNavmeshObject"] = addIsNavmeshObject;
		objectJson["tag"] = "Prop";
		objectJson["layer"] = Layer::Default;
	}
	else if (addTypeIndex == 1)
	{
		objectJson["type"] = "Aracore";
		objectJson["tag"] = "Enemy";
		objectJson["layer"] = Layer::Enemy;
	}
	else if (addTypeIndex == 2)
	{
		objectJson["type"] = "AracoreQueen";
		objectJson["tag"] = "Enemy";
		objectJson["layer"] = Layer::Enemy;
	}

	return objectJson;
}

void StageLoader::DrawGUI()
{
	if (ImGui::TreeNode(ICON_FA_MOUNTAIN "StageLoader"))
	{
		ImGui::Text(
			"Json: %s",
			jsonPath.string().c_str());

		ImGui::Text(
			"Loaded Actors: %d",
			(int)loadedActors.size());

		if (ImGui::Button("Reload"))
		{
			reloadRequested = true;
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

		ImGui::Checkbox(
			"IsNavmeshObject",
			&addIsNavmeshObject);
	}

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
			ImGui::Text("Group: %s", loadedActor.groupName.c_str());

			if (loadedActor.groupName == "props")
			{
				if (loadedActor.isNavmeshObject)
				{
					ApplyNavmeshObjectToProp(
						actor,
						true);
				}

				if (ImGui::Checkbox(
					"IsNavmeshObject",
					&loadedActor.isNavmeshObject))
				{
					loadedActor.objectJson["isNavmeshObject"] =
						loadedActor.isNavmeshObject;

					ApplyNavmeshObjectToProp(
						actor,
						loadedActor.isNavmeshObject);

					if (NavMeshActor* navMeshActor = NavMeshActor::GetActive())
					{
						navMeshActor->RequestBuild(2);
					}
				}

				const bool hasNavmeshObstacle =
					actor->GetComponent<NavMeshObstacle>() != nullptr;

				ImGui::Text(
					"NavMeshObstacle: %s",
					hasNavmeshObstacle ? "Attached" : "Missing");
			}

			const bool dontScale =
				loadedActor.groupName != "props";
			Transform::TransformChangedResult transformResult =
				actor->transform.DrawGUI(dontScale);
			if (transformResult.positionChanged ||
				transformResult.rotationChanged ||
				transformResult.scaleChanged)
			{
				ApplyTransformToActor(
					actor,
					actor->transform,
					dontScale);

				if (loadedActor.isNavmeshObject)
				{
					if (NavMeshActor* navMeshActor = NavMeshActor::GetActive())
					{
						navMeshActor->RequestBuild(1);
					}
				}
			}

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
