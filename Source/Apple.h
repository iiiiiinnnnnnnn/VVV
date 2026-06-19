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

	void OnCollisionEnter(Collider* self, Collider* other, const Vector3& point, const Vector3& normal) override;
	void OnCollisionExit(Collider* self, Collider* other, const Vector3& point, const Vector3& normal) override;
	void OnTriggerEnter(Collider* self, Collider* other, const Vector3& point, const Vector3& normal) override;
	void OnTriggerExit(Collider* self, Collider* other, const Vector3& point, const Vector3& normal) override;

	void OnDead() override;

	void SetAggressive(bool aggressive) { isAggressive = aggressive; }
	bool IsAggressive() const { return isAggressive; }

	void SetPosition(const Vector3& pos);

private:
	bool isAggressive = false;
	Rigidbody* rb;
	DamageHoleComponent* damageHoleComponent = nullptr;
};
