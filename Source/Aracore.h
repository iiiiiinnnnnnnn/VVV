// Aracore.h

#pragma once

#include "Entity.h"

class NavMeshAgent;
class Aracore;

class AracoreMachine : public Entity
{
public:
	AracoreMachine(Aracore* ownerAracore);

	void OnDamaged(const DamageData& damageData) override;

private:
	Aracore* ownerAracore = nullptr;
	DamageHoleComponent* damageHoleComponent = nullptr;
	ShaderParamList shaderParam;
	Collider* collider = nullptr;
};

class Aracore : public Entity
{
public:
	Aracore();
	~Aracore() = default;
	void OnRegistered(ActorManager* actorManager) override;
	void OnUpdate() override;
	void OnDrawGUI() override;

	void OnCollisionEnter(Collider* self, Collider* other, const Vector3& point, const Vector3& normal) override;
	void OnTriggerEnter(Collider* self, Collider* other, const Vector3& point, const Vector3& normal) override;

	void OnDamaged(const DamageData& damageData) override;
	void OnDead() override;

private:
	friend class AracoreMachine;
	Actor* FindPlayer() const;
	void UpdateChase();

	Animator* anim;
	ModelRenderComponent* bodyRenderer = nullptr;
	RigidbodyDynamic* rb;
	std::shared_ptr<Model> model;
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

