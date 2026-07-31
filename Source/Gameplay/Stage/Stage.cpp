// Stage.cpp

#include "Gameplay/Stage/Stage.h"

#include "Gameplay/Stage/Component/StageLoader.h"
#include "Gameplay/Stage/Component/Terrain.h"
#include "Physics/Collider/TerrainMeshCollider.h"
#include "Physics/Navigation/NavMeshActor.h"
#include "Physics/RigidBody/Rigidbody.h"
#include "Resource/ResourceManager.h"
#include "Resource/VSTG.h"

bool Stage::LoadVSTG(const std::string& path)
{
	VSTG data;
	const std::string resolvedPath = ResourceManager::Instance().ResolvePath(path);
	if (!data.Load(resolvedPath))
	{
		vstgError = data.GetError();
		return false;
	}

	auto* rigidbody = AddComponent<RigidbodyStatic>();
	auto* terrain = AddComponent<Terrain>();
	auto* navMesh = AddComponent<NavMeshActor>();
	auto* loader = AddComponent<StageLoader>(this, std::string("{}"), true);
	if (!data.Apply(*terrain, *navMesh, *loader, lightManager))
	{
		vstgError = "VSTG content could not be applied.";
		return false;
	}
	AddComponent<TerrainMeshCollider>(Layers::Get("Terrain"), rigidbody);
	vstgError.clear();
	return true;
}
