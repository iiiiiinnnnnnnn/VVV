// AracoreQueen.h

#pragma once
#include "Rendering/Core/VMatRenderParams.h"
#include <memory>
#include <vector>

#include "Gameplay/Actor/Entity.h"
#include "Gameplay/AI/EnemyAIFlow.h"

class AracoreQueen;
class VMDL;

// component
#include "Physics/RigidBody/RigidbodyDynamic.h"
#include "Physics/Navigation/NavMeshAgent.h"
#include "Animation/Animator.h"
#include "Resource/VMDLModel.h"
#include "Rendering/Component/VMDLModelComponent.h"
#include "Physics/Core/PhysicsComponent.h"
class MultiLegFootIK;

class AracoreQueen : public Entity
{
public:
	AracoreQueen();
	~AracoreQueen() = default;
	void OnUpdate() override;
	void OnDrawGUI() override;

	void OnCollisionEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) override;
	void OnTriggerEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) override;

	void OnDamaged(const DamageData& damageData) override;
	void OnDead(const DamageData& damageData) override;

private:
	void Threat();

	Animator* anim = nullptr;
	VMDL* vmdl = nullptr;
	RigidbodyDynamic* rb = nullptr;
	std::shared_ptr<VMDLModel> model;
	NavMeshAgent* navMeshAgent = nullptr;
	MultiLegFootIK* multiLegFootIK = nullptr;
	std::vector<PhysicsComponent*> IKColliders;
	std::vector<PhysicsComponent*> IKStampColliders;
	EnemyAIFlow* controller = nullptr;
	std::vector<Vector3> colPositions;

	VMatRenderParams renderParams;
};
