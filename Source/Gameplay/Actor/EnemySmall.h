// EnemySmall.h

#pragma once

#include "Gameplay/Actor/Entity.h"

#include "Rendering/Component/VMDL.h"
#include "Animation/Animator.h"
#include "Physics/Collider/CharacterController.h"
#include "Gameplay/Component/CharacterMotorComponent.h"
#include "Animation/LookAt.h"

class EnemySmall : public Entity
{
public:
	EnemySmall(const Vector3& position = Vector3::Zero);
	void OnUpdate() override;
	void OnLateUpdate() override;
	void OnDrawGUI() override;
private:
	void UpdateLookIn();
	VMDL* vmdl;
	std::shared_ptr<VMDLModel> model;
	LookAt* lookAt;
	Animator* anim;
	CharacterController* cc;
	CharacterMotorComponent* motor;
	Actor* lookInTarget = nullptr;
};