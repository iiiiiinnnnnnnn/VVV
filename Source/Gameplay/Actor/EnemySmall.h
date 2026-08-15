#pragma once

#include "Gameplay/Actor/Entity.h"

#include "Rendering/Component/VMDL.h"
#include "Animation/Animator.h"
#include "Physics/Collider/CharacterController.h"
#include "Gameplay/Component/CharacterMotorComponent.h"
#include "Gameplay/AI/EnemyAIFlow.h"
#include "Animation/LookAt.h"

class EnemySmall : public Entity
{
public:
	EnemySmall(const Vector3& position = Vector3::Zero, const Vector3& euler = Vector3::Zero);
	void OnUpdate() override;
	void OnLateUpdate() override;
	void OnDrawGUI() override;
	void OnTriggerEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) override;
private:
	void OnDamaged(const DamageData& damageData) override;
	void OnDead(const DamageData& damageData) override;
	VMDL* vmdl;
	std::shared_ptr<VMDLModel> model;
	Animator* anim;
	EnemyAIFlow* controller;
	NavMeshAgent* navMeshAgent;
	CharacterController* cc;
	bool deathCleanupPending = false;
};
