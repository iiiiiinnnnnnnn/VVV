#pragma once
#include <memory>
#include <string>
#include "Gameplay/Actor/Entity.h"

class VMDL;
class VMDLModel;
class VMDLColliderComponent;
bool GetCrystalModelBounds(const VMDLModel& model, Vector3& center, Vector3& size);

// クリスタル・破片の基底
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

// 破片
class CrystalPart : public BaseCrystal
{
public:
	CrystalPart(const Transform& placement, std::shared_ptr<VMDLModel> model);
	void StartMoving(const Vector3& velocity, const Vector3& rotationSpeed, Actor* source = nullptr);

private:
	void OnUpdate() override;
	void OnCollisionEnter(PhysicsComponent* self, PhysicsComponent* other,
		const Vector3& point, const Vector3& normal) override;
	VMDLColliderComponent* body = nullptr;
	Vector3 initialScale;
	std::weak_ptr<Actor> sourceActor;
	float elapsedTime = 0.0f;
	bool contactPending = false;
	bool shrinking = false;
};

// クリスタル
class Crystal : public BaseCrystal
{
public:
	Crystal(const Transform& transform, const std::string& modelPath = "Crystal");

private:
	void OnUpdate() override;
	void SpawnFragments();
	void OnDamaged(const DamageData& damageData) override;
	void OnDead(const DamageData& damageData) override;
	std::shared_ptr<VMDLModel> partModel;
	Vector3 crystalCenter, crystalSize, partSize;
	bool deathCleanupPending = false;
};
