#pragma once

#include "Application/Time/Timer.h"
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
public:
	~BaseCrystal() override;

protected:
	BaseCrystal(const Transform& placement, const std::string& name, const std::string& modelPath);
	BaseCrystal(const Transform& placement, const std::string& name, std::shared_ptr<VMDLModel> model);
	void OnInit() override;
	void OnUninit() override;
	std::string modelPath;
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
	~CrystalPart() override;
	void StartMoving(const Vector3& velocity, const Vector3& rotationSpeed, Actor* source = nullptr);

private:
	void OnUninit() override;
	void OnUpdate() override;
	void OnCollisionEnter(PhysicsComponent* self, PhysicsComponent* other,
		const Vector3& point, const Vector3& normal) override;
	VMDLColliderComponent* body = nullptr;
	Vector3 initialScale;
	std::weak_ptr<Actor> sourceActor;
	Game::Timer lifetimeTimer{6.0f};
	bool contactPending = false;
	bool shrinking = false;
};

// クリスタル
class Crystal : public BaseCrystal
{
public:
	Crystal(const Transform& transform, const std::string& modelPath = "Crystal");
	~Crystal() override;

private:
	void OnUninit() override;
	void OnInit() override;
	void OnUpdate() override;
	void SpawnFragments();
	void OnDamaged(const DamageData& damageData) override;
	void OnDead(const DamageData& damageData) override;
	std::shared_ptr<VMDLModel> partModel;
	Vector3 crystalCenter, crystalSize, partSize;
	bool deathCleanupPending = false;
};
