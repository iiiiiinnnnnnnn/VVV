#pragma once
#include <memory>
#include <string>
#include "Gameplay/Actor/Entity.h"

class VMDL;
class VMDLModel;
class VMDLColliderComponent;

class BaseCrystal : public Entity
{
protected:
	BaseCrystal(const Transform& placement, const std::string& name, const std::string& modelPath);
	BaseCrystal(const Transform& placement, const std::string& name, std::shared_ptr<VMDLModel> model);
	void OnAwake() override;
	std::shared_ptr<VMDLModel> loadedModel;
	bool PlayBreakSound();
	bool PlayBreakParticle();
	VMDL* vmdl = nullptr;
};

class CrystalPart : public BaseCrystal
{
public:
	CrystalPart(const Transform& placement, std::shared_ptr<VMDLModel> model);
	void StartMoving(const Vector3& velocity, const Vector3& rotationSpeed);

private:
	void OnUpdate() override;
	void OnCollisionEnter(PhysicsComponent* self, PhysicsComponent* other,
		const Vector3& point, const Vector3& normal) override;
	VMDLColliderComponent* body = nullptr;
	Vector3 initialScale;
	float elapsedTime = 0.0f;
	bool contactPending = false;
	bool shrinking = false;
};

class Crystal : public BaseCrystal
{
public:
	Crystal(const Transform& transform, const std::string& modelPath = "Resources/Model/Crystal");

private:
	void OnUpdate() override;
	void SpawnFragments();
	void OnDamaged(const DamageData& damageData) override;
	void OnDead(const DamageData& damageData) override;
	std::shared_ptr<VMDLModel> partModel;
	Vector3 crystalCenter, crystalSize, partSize;
	bool deathCleanupPending = false;
};
