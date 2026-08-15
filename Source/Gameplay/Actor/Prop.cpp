#include "Gameplay/Actor/Prop.h"
#include "Physics/Navigation/NavMeshActor.h"
#include "Physics/Navigation/NavMeshObstacle.h"
#include "Resource/ResourceManager.h"
#include "Physics/RigidBody/Rigidbody.h"
#include "Rendering/Component/VMDLModelComponent.h"
#include "Rendering/Component/DamageHoleComponent.h"
#include "Physics/Collider/MeshCollider.h"
#include "Gameplay/Scene/HitStop.h"
#include "Gameplay/Scene/CameraEffectController.h"

Prop::Prop(StageLoader::PropData& propData) : Actor(propData.name, propData.tag, true)
{
	transform = propData.transform;
	transform.Update();

	if (!propData.model) propData.model = ResourceManager::Instance().LoadModel(propData.modelPath);
	propData.model->UpdateTransform(transform.matrix);
	modelRenderer = AddComponent<VMDLModelComponent>(propData.model, ModelShaderId::VMat);
	modelRenderer->SetAttachmentLayerId(Layers::Get("Prop"));

	if (propData.rigidbodyData.isDynamic)
	{
		rigidbody = AddComponent<RigidbodyDynamic>();
		AddComponent<MeshCollider>(Layers::Get("Prop"), rigidbody, propData.model, true);
	}

	AddComponent<NavMeshObstacle>();

	useDestroy = propData.useDestroy;
	destroyLife = propData.destroyLife;
	destroyLayerMask = propData.destroyLayerMask;
	if (useDestroy && destroyLife > 0.0f)
	{
		damageHoleComponent =
			AddComponent<DamageHoleComponent>(modelRenderer, 2.0f, 2.0f, 2.0f, 1.0f);
	}
}

void Prop::ApplyStageData(StageLoader::PropData& propData)
{
	SetName(propData.name);
	SetTag(propData.tag);
	transform = propData.transform;
	transform.Update();
	if (rigidbody)
	{
		rigidbody->SetPosition(transform.position);
		rigidbody->SetRotation(transform.rotation);
	}

	useDestroy = propData.useDestroy;
	destroyLayerMask = propData.destroyLayerMask;
}

void Prop::OnTriggerEnter(
	PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal)
{
	if (!useDestroy) return;
	if (destroyLife <= 0.0f) return;
	const LayerId layer = other->GetLayerId();
	if (layer >= EditableLayerCount || (destroyLayerMask & (1u << layer)) == 0) return;

	HitStop::Request(0.1f);
	CameraEffectController::Request(0.1f, 0.1f);

	if (damageHoleComponent) damageHoleComponent->AddDamageHoleFromPosition(point, -normal);
	destroyLife -= 1.0f;

	if (destroyLife <= 0.0f)
	{
		Destroy(0);
		destroyLife = 0.0f;
		if (NavMeshActor* navMeshActor = NavMeshActor::GetActive()) navMeshActor->RequestBuild();
	}
}
