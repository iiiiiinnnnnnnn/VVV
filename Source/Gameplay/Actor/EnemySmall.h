// EnemySmall.h

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
	EnemySmall(const Vector3& position = Vector3::Zero);
	void OnUpdate() override;
	void OnLateUpdate() override;
	void OnDrawGUI() override;
	void OnCollisionEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) override;
private:
	void OnDamaged(const DamageData& damageData) override;
	VMDL* vmdl;
	std::shared_ptr<VMDLModel> model;
	Animator* anim;
	EnemyAIFlow* controller;
	NavMeshAgent* navMeshAgent;
	CharacterController* cc;
	CharacterMotorComponent* motor;
	float freedomWaitTimer = 0.0f;
	float freedomWaitDuration = 2.0f;
	float freedomMinDistance = 3.0f;
	float freedomMaxDistance = 10.0f;
	float freedomMoveSpeed = 1.5f;
	float attackMoveSpeed = 3.5f;
};
