// Apple.h

#pragma once

#include "Entity.h"
#include "ModelRenderComponent.h"

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
	static constexpr int MaxDamageHoles = 8;

	void AddDamageHoleFrom(Actor* attacker);
	void UpdateDamageHoleShaderParams();

	bool isAggressive = false;
	Rigidbody* rb;
	ModelRenderComponent* modelRenderer = nullptr;
	std::vector<Vector4> damageHoles;
	float damageHoleRadius = 13.0f;
	float damageHoleEdgeWidth = 2.5f;
};
