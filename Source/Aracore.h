// Aracore.h

#pragma once

#include "Entity.h"

class Aracore : public Entity
{
public:
	Aracore();
	~Aracore() = default;
	void OnRegistered(ActorManager* actorManager) override;
	void OnUpdate() override;
	void OnDrawGUI() override;

	void OnDamaged(const DamageData& damageData) override;

	void OnCollisionEnter(Actor* other) override;
	void OnCollisionExit(Actor* other) override;
	void OnTriggerEnter(Actor* other) override;
	void OnTriggerExit(Actor* other) override;

	void OnDead() override;

private:
	Animator* anim;
	Rigidbody* rb;
	DamageHoleComponent* damageHoleComponent = nullptr;
};
