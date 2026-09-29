#pragma once

#include "Gameplay/Actor/Entity.h"

class MeshCollider;
class PhysicsComponent;
class Rigidbody;
class VMDL;

class Crystal : public Entity
{
  public:
	Crystal(const Transform& transform);
	~Crystal() override = default;

  private:
	void Break();
	void PlayBreakSound();
	void OnDamaged(const DamageData& damageData) override;
	void OnDead(const DamageData& damageData) override;

	VMDL* vmdl = nullptr;
	bool broken = false;
};
