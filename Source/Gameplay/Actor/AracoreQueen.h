// AracoreQueen.h

#pragma once
#include "Rendering/Core/VMatRenderParams.h"
#include <memory>
#include <vector>

#include "Gameplay/Actor/Entity.h"
#include "Gameplay/AI/EnemyAIFlow.h"

class AracoreQueen;

// component
#include "Physics/RigidBody/RigidbodyDynamic.h"
#include "Physics/Navigation/NavMeshAgent.h"
#include "Animation/Animator.h"
#include "Rendering/Component/DamageHoleComponent.h"
#include "Resource/VMDLModel.h"
#include "Rendering/Component/VMDLModelComponent.h"
#include "Physics/Core/PhysicsComponent.h"
#include "Animation/SpiderFootIK.h"

class AracoreQueenMachine : public Entity
{
public:
	AracoreQueenMachine(AracoreQueen* ownerAracoreQueen);

	void OnDamaged(const DamageData& damageData) override;
	void OnDead(const DamageData& damageData) override;

private:
	class AracoreQueen* ownerAracoreQueen = nullptr;
	DamageHoleComponent* damageHoleComponent = nullptr;
	VMatRenderParams renderParams;
	PhysicsComponent* collider = nullptr;
};

class AracoreQueen : public Entity
{
public:
	AracoreQueen();
	~AracoreQueen() = default;
	void OnAwake() override;
	void OnUpdate() override;
	void OnDrawGUI() override;

	void OnCollisionEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) override;
	void OnTriggerEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) override;

	void OnDamaged(const DamageData& damageData) override;
	void OnDead(const DamageData& damageData) override;

private:
	friend class AracoreQueenMachine;
	Animator* anim = nullptr;
	VMDLModelComponent* bodyRenderer = nullptr;
	RigidbodyDynamic* rb = nullptr;
	std::shared_ptr<VMDLModel> model;
	Actor* machine = nullptr;
	NavMeshAgent* navMeshAgent = nullptr;
	SpiderFootIK* spiderFootIK = nullptr;
	PhysicsComponent* bodyCollider = nullptr;
	std::vector<PhysicsComponent*> IKColliders;
	std::vector<PhysicsComponent*> IKStampColliders;
	EnemyAIFlow* controller = nullptr;
	DamageHoleComponent* damageHoleComponent = nullptr;
	std::vector<Vector3> colPositions;

	VMatRenderParams renderParams;
};
