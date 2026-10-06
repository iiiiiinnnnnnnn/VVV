#pragma once
#include <string>

#include "Gameplay/Actor/Entity.h"

class MeshCollider;
class PhysicsComponent;
class Rigidbody;
class VMDL;

class Crystal : public Entity
{
  public:
	Crystal(const Transform& transform, const std::string& modelPath = "Resources/Model/Crystal");
	~Crystal() override = default;

  private:
	void OnUpdate() override;
	void PlayBreakSound();
	void OnDamaged(const DamageData& damageData) override;
	void OnDead(const DamageData& damageData) override;

	VMDL* vmdl = nullptr;
	bool deathCleanupPending = false;
};
