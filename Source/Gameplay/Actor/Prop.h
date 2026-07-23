// Prop.h

#pragma once

#include "Gameplay/Actor/Actor.h"
#include "Gameplay/Stage/Component/StageLoader.h"

class DamageHoleComponent;
class VMDLModelComponent;

class Prop : public Actor
{
public:
	Prop(StageLoader::PropData& propData);
	~Prop() = default;

	void ApplyStageData(StageLoader::PropData& propData);

	void OnTriggerEnter(
		PhysicsComponent* self,
		PhysicsComponent* other,
		const Vector3& point,
		const Vector3& normal) override;

	DamageHoleComponent* damageHoleComponent = nullptr;
	VMDLModelComponent* modelRenderer = nullptr;
	bool useDestroy = false;
	float destroyLife = 0.0f;
};
