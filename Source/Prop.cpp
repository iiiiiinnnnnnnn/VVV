// Prop.cpp

#include "Prop.h"
#include "ResourceManager.h"

Prop::Prop(StageLoader::PropData& propData)
	: Actor("Prop", "Prop", true, Layer::Prop)
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

	AddComponent<BoxCollider>(rb, propData.boxColliderData.size, propData.boxColliderData.localPosition,
		PhysicsManager::Instance().GetPhysics()->createMaterial(
		propData.boxColliderData.staticFriction, propData.boxColliderData.dynamicFriction, propData.boxColliderData.restitution));
}

void Prop::OnUpdate()
{

}