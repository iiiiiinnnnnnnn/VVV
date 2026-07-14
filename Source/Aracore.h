// Aracore.h

#pragma once
#include "ShaderParam.h"
#include <memory>
#include <vector>

#include "Entity.h"
#include "ParticleSystem.h"

class NavMeshAgent;
class Actor;
class ActorManager;
class Animator;
class DamageHoleComponent;
class Model;
class ModelRenderComponent;
class PhysicsComponent;
class RigidbodyDynamic;
class Aracore;
class SpiderFootIK;

class AracoreMachine : public Entity
{
public:
	AracoreMachine(Aracore* ownerAracore);

	void OnDamaged(const DamageData& damageData) override;
	void OnDead(const DamageData& damageData) override;

private:
	Aracore* ownerAracore = nullptr;
	DamageHoleComponent* damageHoleComponent = nullptr;
	ShaderParamListWithMaterialName shaderParamWithMaterialName;
	PhysicsComponent* collider = nullptr;
};

class Aracore : public Entity
{
public:
	Aracore(const Vector3& position);
	~Aracore() = default;
	void OnAwake() override;
	void OnUpdate() override;
	void OnDrawGUI() override;

	void SetBreakParticleSystem(ParticleSystem* particleSystem) { breakParticleSystem = particleSystem; }

	void OnCollisionEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) override;
	void OnTriggerEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) override;

	void OnDamaged(const DamageData& damageData) override;
	void OnDead(const DamageData& damageData) override;

private:
	friend class AracoreMachine;
	void UpdateChase();
	void SpawnBreakParticles();

	Animator* anim = nullptr;
	ParticleSystem* breakParticleSystem;
	ModelRenderComponent* bodyRenderer = nullptr;
	RigidbodyDynamic* rb = nullptr;
	std::shared_ptr<Model> model;
	Actor* machine = nullptr;
	NavMeshAgent* navMeshAgent = nullptr;
	SpiderFootIK* spiderFootIK = nullptr;
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
