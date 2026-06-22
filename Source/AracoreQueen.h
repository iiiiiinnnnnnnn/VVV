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
	Collider* collider = nullptr;
};

class AracoreQueen : public Entity
{
public:
	AracoreQueen();
	~AracoreQueen() = default;
	void OnRegistered(ActorManager* actorManager) override;
	void OnUpdate() override;
	void OnDrawGUI() override;

	void OnCollisionEnter(Collider* self, Collider* other, const Vector3& point, const Vector3& normal) override;
	void OnTriggerEnter(Collider* self, Collider* other, const Vector3& point, const Vector3& normal) override;

	void OnDamaged(const DamageData& damageData) override;
	void OnDead() override;

private:
	friend class AracoreQueenMachine;
	Actor* FindPlayer() const;
	void UpdateChase();

	Animator* anim;
	ModelRenderComponent* bodyRenderer = nullptr;
	RigidbodyDynamic* rb;
	std::shared_ptr<Model> model;
	Actor* machine = nullptr;
	NavMeshAgent* navMeshAgent = nullptr;
	Collider* bodyCollider = nullptr;
	std::vector<Collider*> IKColliders;
	std::vector<Collider*> IKStampColliders;
	DamageHoleComponent* damageHoleComponent = nullptr;
	bool chasingPlayer = false;
	float chaseStartDistance = 18.0f;
	float chaseStopDistance = 24.0f;

	ShaderParamListWithMaterialName shaderParamWithMaterialName;
};

