// Prop.cpp

#include "Prop.h"
#include "NavMeshActor.h"
#include "NavMeshObstacle.h"
#include "ResourceManager.h"
#include "Rigidbody.h"
#include "ModelRenderComponent.h"
#include "BoxCollider.h"

Prop::Prop(StageLoader::PropData& propData)
	: Actor("Prop", "Prop", true)
{
	propData.transform.Update();
	transform = propData.transform;

	Rigidbody* rb;
	if (propData.rigidbodyData.isDynamic)
		rb = AddComponent<RigidbodyDynamic>();
	else
		rb = AddComponent<RigidbodyStatic>();

	propData.model = ResourceManager::Instance().LoadModel(propData.modelPath);
	AddComponent<ModelRenderComponent>(propData.model, ModelShaderId::PBR, propData.shaderParams);

	AddComponent<BoxCollider>(Layers::Get("Prop"), rb, propData.boxColliderData.size, propData.boxColliderData.localPosition,
		PhysicsManager::Instance().GetPhysics()->createMaterial(
		propData.boxColliderData.staticFriction, propData.boxColliderData.dynamicFriction, propData.boxColliderData.restitution));

	AddComponent<NavMeshObstacle>();
	if (NavMeshActor* navMeshActor = NavMeshActor::GetActive())
		navMeshActor->RequestBuild();
}

void Prop::OnUpdate()
{

}
