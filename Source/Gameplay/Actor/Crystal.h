#pragma once

#include <memory>
#include <string>
#include "Gameplay/Actor/Entity.h"

class VMDL;
class VMDLModel;
class HitstopVibrationComponent;

class Crystal : public Entity
{
public:
	Crystal(const Transform& transform, const std::string& modelPath = "Crystal");
	~Crystal() override;

private:
	void OnAwake() override;
	void OnUpdate() override;
	void OnDamaged(const DamageData& damageData) override;
	void OnDead(const DamageData& damageData) override;
	bool PlayBreakSound();
	bool PlayBreakParticle();

	std::string modelPath;
	std::shared_ptr<VMDLModel> loadedModel;
	VMDL* vmdl = nullptr;
	HitstopVibrationComponent* hitstopVibration = nullptr;
	Vector3 crystalCenter = Vector3::Zero;
	Vector3 crystalSize = Vector3::Zero;
	bool deathCleanupPending = false;
};
