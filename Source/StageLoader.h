// StageLoader.h

#pragma once

#include "Common.h"
#include "Component.h"
#include "Actor.h"
#include "nlohmann/json.hpp"

class StageLoader : public Component
{
public:
	StageLoader(Object* owner, Actor* stage, std::filesystem::path jsonPath);
	~StageLoader() = default;

	void Update() override;
	void DrawGUI() override;

	template<class T>
	T* GetActor(const std::string& id)
	{
		auto it = namedActors.find(id);

		if (it == namedActors.end())
		{
			return nullptr;
		}

		return dynamic_cast<T*>(it->second);
	}

private:
	using json = nlohmann::json;

	struct LoadedActor
	{
		std::shared_ptr<Actor> actor;
		std::string groupName;
		json objectJson;
	};

	void LoadStageFromJSON();
	void SaveStageToJSON();
	void ClearLoadedActors();

	Actor* SpawnActorFromJSON(
		const json& objectJson,
		const std::string& groupName);

	json BuildStageJSONFromLoadedActors() const;
	json BuildObjectJSONFromLoadedActor(
		const LoadedActor& loadedActor) const;

	void WriteTransformToJSON(
		json& objectJson,
		const Transform& transform) const;

	Transform ReadTransform(
		const json& objectJson) const;

	Vector3 ReadVector3(
		const json& objectJson,
		const char* key,
		const Vector3& defaultValue) const;

	json MakeAddObjectJSON() const;

	void DrawAddObjectGUI();
	void DrawLoadedActorsGUI();

	void ApplyTransformToActor(
		Actor* actor,
		const Transform& transform);

	void ApplyCommonActorSettings(
		Actor* actor,
		const json& objectJson);

	Actor* stage = nullptr;
	std::filesystem::path jsonPath;

	bool loaded = false;

	json stageJson;

	std::vector<LoadedActor> loadedActors;
	std::unordered_map<std::string, Actor*> namedActors;

	int addTypeIndex = 1;
	std::string addId = "aracore_001";
	std::string addPropPath = "Data/Model/Prop/paestum_stone.glb";

	Vector3 addPosition = Vector3::Zero;
	Vector3 addRotation = Vector3::Zero;
	Vector3 addScale = Vector3::One;

	bool addAggressive = false;
	bool addIsDynamic = false;
	int addMeshColliderConvex = 1280;
};
