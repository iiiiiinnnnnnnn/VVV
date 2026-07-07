// AracoreQueen.h

#pragma once
#include "ShaderParam.h"
#include <memory>
#include <vector>

#include "Entity.h"

class NavMeshAgent;
class Actor;
class ActorManager;
class Animator;
class DamageHoleComponent;
class Model;
class ModelRenderComponent;
class PhysicsComponent;
class RigidbodyDynamic;
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
	void OnDrawGUI() override;

	void OnCollisionEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) override;
	void OnTriggerEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) override;

	void OnDamaged(const DamageData& damageData) override;
	void OnDead() override;

private:
	friend class AracoreQueenMachine;
	void UpdateChase();

	Animator* anim = nullptr;
	ModelRenderComponent* bodyRenderer = nullptr;
	RigidbodyDynamic* rb = nullptr;
	std::shared_ptr<Model> model;
	Actor* machine = nullptr;
	NavMeshAgent* navMeshAgent = nullptr;
	PhysicsComponent* bodyCollider = nullptr;
	std::vector<PhysicsComponent*> IKColliders;
	std::vector<PhysicsComponent*> IKStampColliders;
	DamageHoleComponent* damageHoleComponent = nullptr;
	enum class ChaseType
	{
		No, Walk, Run
	} chasingPlayer = ChaseType::No;
	float chaisedTimer = 0.0f;
	float navAgentRadius = 2.17f;
	std::vector<Vector3> colPositions;

	ShaderParamListWithMaterialName shaderParamWithMaterialName;
};
