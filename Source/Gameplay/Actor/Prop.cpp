// Prop.cpp

#include "Gameplay/Actor/Prop.h"
#include "Physics/Navigation/NavMeshActor.h"
#include "Physics/Navigation/NavMeshObstacle.h"
#include "Resource/ResourceManager.h"
#include "Physics/RigidBody/Rigidbody.h"
#include "Rendering/Component/VMDLModelComponent.h"
#include "Physics/Collider/BoxCollider.h"
#include "Rendering/Component/DamageHoleComponent.h"
#include "Physics/Collider/MeshCollider.h"
#include "Gameplay/Scene/HitStop.h"
#include "Gameplay/Scene/CameraEffectController.h"

Prop::Prop(StageLoader::PropData& propData)
	: Actor("Prop", "Prop", true)
{
	transform = propData.transform;
	transform.Update();

	propData.model = ResourceManager::Instance().LoadModel(propData.modelPath);
	propData.model->UpdateTransform(transform.matrix);
	modelRenderer = AddComponent<VMDLModelComponent>(propData.model, ModelShaderId::PBR, propData.shaderParams);
	modelRenderer->SetShaderParamForAllMaterials(propData.MakePBRParams());

	if (propData.colliderType == StageLoader::ColliderType::Mesh)
	{
		Rigidbody* rb;
		rb = AddComponent<RigidbodyStatic>();
		AddComponent<MeshCollider>(Layers::Get("Prop"), rb, propData.model.get());
	}
	else
	{
		Rigidbody* rb;
		if (propData.rigidbodyData.isDynamic)
			rb = AddComponent<RigidbodyDynamic>();
		else
			rb = AddComponent<RigidbodyStatic>();

		AddComponent<BoxCollider>(Layers::Get("Prop"), rb, propData.boxColliderData.size, propData.boxColliderData.localPosition,
			PhysicsManager::Instance().GetPhysics()->createMaterial(
			propData.boxColliderData.staticFriction, propData.boxColliderData.dynamicFriction, propData.boxColliderData.restitution));
	}

	AddComponent<NavMeshObstacle>();
	if (NavMeshActor* navMeshActor = NavMeshActor::GetActive())
		navMeshActor->RequestBuild();

	useDestroy = propData.useDestroy;
	destroyLife = propData.destroyLife;
	if (useDestroy && destroyLife > 0.0f)
	{
		damageHoleComponent =
			AddComponent<DamageHoleComponent>(modelRenderer, 2.0f, 2.0f, 2.0f, 1.0f);
	}
}

void Prop::ApplyStageData(StageLoader::PropData& propData)
{
	transform = propData.transform;
	transform.Update();

	if (modelRenderer)
	{
		modelRenderer->SetShaderParamForAllMaterials(propData.MakePBRParams());
	}
}

void Prop::OnTriggerEnter(
	PhysicsComponent* self,
	PhysicsComponent* other,
	const Vector3& point,
	const Vector3& normal)
{
	if (!useDestroy) return;
	if (destroyLife <= 0.0f) return;
	if (other->GetLayerId() != Layers::Get("PlayerAtk") &&
		other->GetLayerId() != Layers::Get("EnemyAtk")) return;

	HitStop::Request(0.1f);
	CameraEffectController::Request(0.1f, 0.1f);

	if (damageHoleComponent)
		damageHoleComponent->AddDamageHoleFromPosition(point, -normal);
	destroyLife -= 1.0f;

	if (destroyLife <= 0.0f)
	{
		Destroy(0);
		destroyLife = 0.0f;
	}
}
