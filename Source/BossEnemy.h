// BossEnemy.h

#pragma once

#include "Entity.h"

class BossEnemy : public Entity
{
public:
	BossEnemy();
	~BossEnemy() = default;
	void OnUpdate() override;
	void OnDrawGUI() override;

	void OnDamaged(const DamageData& damageData) override;

	void OnCollisionEnter(Actor* other) override;
	void OnCollisionExit(Actor* other) override;
	void OnTriggerEnter(Actor* other) override;
	void OnTriggerExit(Actor* other) override;

	void OnDead() override;

	void SetAggressive(bool aggressive) { isAggressive = aggressive; }
	bool IsAggressive() const { return isAggressive; }

private:
	bool isAggressive = false;
	Rigidbody* rb;
	DamageHoleComponent* damageHoleComponent = nullptr;
};

