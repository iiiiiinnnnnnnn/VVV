// Apple.h

#pragma once

#include "Entity.h"

class DamageHoleComponent;

class Apple : public Entity
{
public:
	Apple();
	~Apple() = default;
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

	void SetPosition(const Vector3& pos);

private:
	bool isAggressive = false;
	Rigidbody* rb;
	DamageHoleComponent* damageHoleComponent = nullptr;
};
