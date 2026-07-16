// Aracore.h

#pragma once
#include "Rendering/Core/ShaderParam.h"
#include <memory>
#include <vector>

#include "Gameplay/Actor/Entity.h"
#include "Rendering/Effect/ParticleSystem.h"
#include "Gameplay/AI/EnemyAIFlow.h"

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

	void OnCollisionEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) override;
	void OnTriggerEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) override;

	void OnDamaged(const DamageData& damageData) override;
	void OnDead(const DamageData& damageData) override;

	EnemyAIFlow* GetController() const { return controller; }

	void SetBreakParticleSystem(ParticleSystem* particleSystem) { breakParticleSystem = particleSystem; }

private:
	friend class AracoreMachine;
	void SpawnBreakParticles();

	Animator* anim = nullptr;
	ParticleSystem* breakParticleSystem = nullptr;
	ModelRenderComponent* bodyRenderer = nullptr;
	RigidbodyDynamic* rb = nullptr;
	std::shared_ptr<Model> model;
	Actor* machine = nullptr;
	NavMeshAgent* navMeshAgent = nullptr;
	SpiderFootIK* spiderFootIK = nullptr;
	PhysicsComponent* bodyCollider = nullptr;
	std::vector<PhysicsComponent*> IKColliders;
	std::vector<PhysicsComponent*> IKStampColliders;
	EnemyAIFlow* controller = nullptr;
	DamageHoleComponent* damageHoleComponent = nullptr;
	std::vector<Vector3> colPositions;

	ShaderParamListWithMaterialName shaderParamWithMaterialName;
};
