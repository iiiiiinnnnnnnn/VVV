#include "pch.h"
#include "RigidRock.h"
#include "Physics/RigidBody/RigidbodyDynamic.h"
#include "Physics/Collider/MeshCollider.h"

RigidRock::RigidRock(const Transform& transform, const std::string& modelPath) 
	: Actor("RigidRock", "Prop", true, transform), modelPath(modelPath)
{
}

RigidRock::~RigidRock() {
	Uninit();
}

void RigidRock::OnAwake()
{
	vmdl = AddComponent<VMDL>(modelPath, false);
	vmdl->Awake();
	vmdl->GetRenderer()->SetAttachmentLayerId(Layers::Get("Prop"));
	auto* rigidbody = AddComponent<RigidbodyDynamic>();
	rigidbody->SetMaxDepenetrationVelocity(1.0f);
	auto* collider =
		AddComponent<MeshCollider>(Layers::Get("Prop"), rigidbody, vmdl->GetSharedModel(), true);
	PxU32 ignoredLayers = 0;
	for (const char* layerName : {"Player", "Enemy", "PlayerAtk", "EnemyAtk", "EnemyAccessory"})
	{
		const LayerId layer = Layers::Get(layerName);
		if (layer < EditableLayerCount) ignoredLayers |= 1u << layer;
	}
	collider->SetSimulationIgnoredLayers(ignoredLayers);
}
