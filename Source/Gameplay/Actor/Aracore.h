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
class CharacterController;
class BoneSphereCollider;
class Model;
class ModelRenderComponent;
class PhysicsComponent;
class RigidbodyDynamic;
class Aracore;
class SpiderFootIK;

class AracoreMachine : public Actor
{
public:
	AracoreMachine(Aracore* ownerAracore);

private:
	ShaderParamListWithMaterialName shaderParamWithMaterialName;
};

class Aracore : public Entity
{
public:
	Aracore(const Vector3& position);
	~Aracore() = default;
	void Destroy(float delay = 0.0f) override;
	void OnAwake() override;
	void OnUpdate() override;
	void OnDrawGUI() override;

	void OnCollisionEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) override;
	void OnTriggerEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) override;
	void OnTriggerStay(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) override;

	void OnDamaged(const DamageData& damageData) override;
	void OnDead(const DamageData& damageData) override;

	EnemyAIFlow* GetController() const { return controller; }

	void SetBreakParticleSystem(ParticleSystem* particleSystem) { breakParticleSystem = particleSystem; }

private:
	friend class AracoreMachine;
	void BeginAttack();
	void EndAttack();
	void OnEnterAttackHit();
	void OnExitAttackHit();
	void TryDealAttackDamage(Actor* actor, PhysicsComponent* otherCollider, const Vector3& point, const Vector3& normal);
	void SpawnAttackRangeParticles();
	void SpawnBreakCrystalParticles();
	void SpawnBreakSpiderParticles();

	Animator* anim = nullptr;
	CharacterController* characterController = nullptr;
	ParticleSystem* breakParticleSystem = nullptr;
	ModelRenderComponent* bodyRenderer = nullptr;
	RigidbodyDynamic* rb = nullptr;
	std::shared_ptr<Model> model;
	Actor* machine = nullptr;
	NavMeshAgent* navMeshAgent = nullptr;
	SpiderFootIK* spiderFootIK = nullptr;
	PhysicsComponent* bodyCollider = nullptr;
	BoneSphereCollider* attackCollider = nullptr;
	std::vector<PhysicsComponent*> IKColliders;
	std::vector<PhysicsComponent*> IKStampColliders;
	EnemyAIFlow* controller = nullptr;
	std::vector<Vector3> colPositions;
	float deathTimer = 0.0f;
	float attackCooldown = 0.0f;
	float attackRequestTimer = 0.0f;
	bool attackRequested = false;
	bool attackStateEntered = false;
	bool attackHit = false;
	bool deathSequenceActive = false;

	ShaderParamListWithMaterialName shaderParamWithMaterialName;
};
