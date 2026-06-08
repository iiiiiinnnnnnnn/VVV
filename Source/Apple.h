// Apple.h

#pragma once

#include "Entity.h"

class Apple : public Entity
{
public:
	Apple();
	~Apple() = default;
	void OnUpdate() override;
	void OnDrawGUI() override;

	void OnDamaged(float damage, KnockBackData knockBackData) override;

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
};
