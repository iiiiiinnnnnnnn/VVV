// Prop.h

#pragma once

#include "Actor.h"
#include "StageLoader.h"

class DamageHoleComponent;

class Prop : public Actor
{
public:
	Prop(StageLoader::PropData& propData);
	~Prop() = default;

	void OnTriggerEnter(
		PhysicsComponent* self,
		PhysicsComponent* other,
		const Vector3& point,
		const Vector3& normal) override;

	DamageHoleComponent* damageHoleComponent = nullptr;
	bool useDestroy = false;
	float destroyLife = 0.0f;
};
