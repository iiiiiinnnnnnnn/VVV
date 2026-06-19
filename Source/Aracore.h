// Aracore.h

#pragma once

#include "Entity.h"

class Aracore : public Entity
{
public:
	Aracore();
	~Aracore() = default;
	void OnRegistered(ActorManager* actorManager) override;
	void OnUpdate() override;
	void OnDrawGUI() override;

	void OnDamaged(const DamageData& damageData) override;

	void OnCollisionEnter(Collider* self, Collider* other, const Vector3& point, const Vector3& normal) override;
	void OnCollisionExit(Collider* self, Collider* other, const Vector3& point, const Vector3& normal) override;
	void OnTriggerEnter(Collider* self, Collider* other, const Vector3& point, const Vector3& normal) override;
	void OnTriggerExit(Collider* self, Collider* other, const Vector3& point, const Vector3& normal) override;

	void OnDead() override;

private:
	Animator* anim;
	ModelRenderComponent* bodyRenderer = nullptr;
	RigidbodyDynamic* rb;
	Collider* bodyCollider = nullptr;
	std::vector<Collider*> IKColliders;
	DamageHoleComponent* damageHoleComponent = nullptr;

	ShaderParamListWithMaterialName shaderParamWithMaterialName;
	ShaderParamList machineShaderParam;
};
