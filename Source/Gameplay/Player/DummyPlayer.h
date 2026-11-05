#pragma once

#include "Animation/Animator.h"
#include "Gameplay/Actor/Entity.h"
#include "Rendering/Component/VMDL.h"

class ThirdPersonCameraController;
class CharacterMotorComponent;
class LockOnComponent;
class AfterimageComponent;
class Effect;
class VMDLColliderComponent;

class DummyPlayer : public Entity
{
  public:
	DummyPlayer();
	~DummyPlayer() override;

	Animator* GetAnimator() const { return anim; }

  protected:
	void OnAwake() override;

	VMDL* vmdl = nullptr;
	Animator* anim = nullptr;
};