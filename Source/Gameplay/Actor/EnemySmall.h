// EnemySmall.h

#pragma once

#include "Gameplay/Actor/Entity.h"

#include "Rendering/Component/VMDL.h"
#include "Animation/Animator.h"
#include "Physics/Collider/CharacterController.h"

class EnemySmall : public Entity
{
public:
	EnemySmall(const Vector3& position = Vector3::Zero);

private:
	VMDL* vmdl;
	Animator* anim;
	CharacterController* cc;
};