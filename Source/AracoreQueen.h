// AracoreQueen.h

#pragma once

#include "Entity.h"

class NavMeshAgent;
class AracoreQueen;

class AracoreQueenMachine : public Entity
{
public:
	AracoreQueenMachine(AracoreQueen* ownerAracoreQueen);

	void OnDamaged(const DamageData& damageData) override;
	void OnDead() override;

private:
	AracoreQueen* ownerAracoreQueen = nullptr;
	DamageHoleComponent* damageHoleComponent = nullptr;
	ShaderParamListWithMaterialName shaderParamWithMaterialName;
	PhysicsComponent* collider = nullptr;
};

class AracoreQueen : public Entity
{
public:
	AracoreQueen();
	~AracoreQueen() = default;
	void OnRegistered(ActorManager* actorManager) override;
	void OnUpdate() override;
	void OnLateUpdate() override;
	void OnDrawGUI() override;

	void OnCollisionEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) override;
	void OnTriggerEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) override;

	void OnDamaged(const DamageData& damageData) override;
	void OnDead() override;

private:
	friend class AracoreQueenMachine;
	void UpdateChase();

	Animator* anim;
	ModelRenderComponent* bodyRenderer = nullptr;
	RigidbodyDynamic* rb;
	std::shared_ptr<Model> model;
	Actor* machine = nullptr;
	NavMeshAgent* navMeshAgent = nullptr;
	PhysicsComponent* bodyCollider = nullptr;
	std::vector<PhysicsComponent*> IKColliders;
	std::vector<PhysicsComponent*> IKStampColliders;
	DamageHoleComponent* damageHoleComponent = nullptr;
	bool chasingPlayer = false;
	std::vector<FootIK*> footIKs;

	ShaderParamListWithMaterialName shaderParamWithMaterialName;
};
