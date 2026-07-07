// Prop.cpp

#include "Prop.h"
#include "NavMeshActor.h"
#include "NavMeshObstacle.h"
#include "ResourceManager.h"
#include "Rigidbody.h"
#include "ModelRenderComponent.h"
#include "BoxCollider.h"
#include "DamageHoleComponent.h"
#include "MeshCollider.h"
#include "HitStop.h"
#include "CameraEffectController.h"

Prop::Prop(StageLoader::PropData& propData)
	: Actor("Prop", "Prop", true)
{
	transform = propData.transform;
	transform.Update();

	propData.model = ResourceManager::Instance().LoadModel(propData.modelPath);
	propData.model->UpdateTransform(transform.matrix);
	auto mr = AddComponent<ModelRenderComponent>(propData.model, ModelShaderId::PBR, propData.shaderParams);
	mr->SetShaderParamForAllMaterials(propData.MakePBRParams());

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
			AddComponent<DamageHoleComponent>(mr, 2.0f, 2.0f, 2.0f, 1.0f);
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

	// ƒ{ƒRƒb
	if (damageHoleComponent)
		damageHoleComponent->AddDamageHoleFromPosition(point, -normal);
	destroyLife -= 1.0f;

	if (destroyLife <= 0.0f)
	{
		Destroy(0);
		destroyLife = 0.0f;
	}
}

