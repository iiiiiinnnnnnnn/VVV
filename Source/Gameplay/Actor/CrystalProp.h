// CrystalProp.h

#pragma once

#include <functional>
#include <memory>

#include "Gameplay/Actor/Entity.h"
#include "Resource/VMDLModel.h"
#include "Physics/Core/PhysicsComponent.h"
#include "Gameplay/Stage/Component/StageLoader.h"

class MeshCollider;
class DamageHoleComponent;
class VMDLModelComponent;
class ParticleSystem;
class PhysicsComponent;
class Rigidbody;

class CrystalProp : public Entity
{
public:
    CrystalProp(const StageLoader::CrystalData& crystalData);
    ~CrystalProp() override = default;

    void ApplyStageData(const StageLoader::CrystalData& crystalData);
    void SetBreakParticleSystem(ParticleSystem* particleSystem) { breakParticleSystem = particleSystem; }
    void SetDestroyedCallback(std::function<void(CrystalProp*)> callback) { destroyedCallback = std::move(callback); }
    void Update() override;
	void OnTriggerEnter(
		PhysicsComponent* self,
		PhysicsComponent* other,
		const Vector3& point,
		const Vector3& normal) override;
private:
	void Break();
    void SpawnBreakParticles();
    void OnDamaged(const DamageData& damageData) override;
    void OnDead(const DamageData& damageData) override;

    std::shared_ptr<VMDLModel> model;
    ParticleSystem* breakParticleSystem = nullptr;
    std::function<void(CrystalProp*)> destroyedCallback = {};
    Rigidbody* rigidbody = nullptr;
    MeshCollider* meshCollider = nullptr;
    VMDLModelComponent* modelRenderer = nullptr;
    DamageHoleComponent* damageHoleComponent = nullptr;
};
