#pragma once

#include "Gameplay/Actor/Entity.h"

#include "Rendering/Component/VMDL.h"
#include "Animation/Animator.h"
#include "Physics/Collider/CharacterController.h"
#include "Gameplay/Component/CharacterMotorComponent.h"
#include "Gameplay/AI/EnemyAI.h"
#include "Animation/LookAt.h"

class Deer : public Entity
{
public:
	Deer(const Transform& transform, const std::string& modelPath = "Deer", bool inBossField = false);
	~Deer() override;
	void OnUpdate() override;
	void OnLateUpdate() override;
	void OnDrawGUI() override;
	void OnCollisionEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) override;
	void OnTriggerEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) override;

  private:
	void BreakCrystalOnContact(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal);
	void OnInit() override;
	void OnUninit() override;
	void ConfigureAI();
	void StopCharge();
	void OnDamaged(const DamageData& damageData) override;
	void OnDead(const DamageData& damageData) override;
	std::string modelPath;
	VMDL* vmdl = nullptr;
	std::shared_ptr<VMDLModel> model;
	Animator* anim = nullptr;
	EnemyAI* controller = nullptr;
	NavMeshAgent* navMeshAgent = nullptr;
	CharacterController* cc = nullptr;
	Vector3 chargeDirection = Vector3::Zero;
	float normalSlopeLimit = 70.0f;
	bool charging = false;
	bool deathCleanupPending = false;
};
