// CrystalProp.h

#pragma once

#include <functional>
#include <memory>

#include "Gameplay/Actor/Entity.h"
#include "Resource/Model.h"
#include "Physics/Core/PhysicsComponent.h"
#include "Rendering/Core/ShaderParam.h"
#include "Gameplay/Stage/Component/StageLoader.h"

class MeshCollider;
class DamageHoleComponent;
class ModelRenderComponent;
class ParticleSystem;
class PhysicsComponent;
class Rigidbody;

class CrystalProp : public Entity
{
public:
    CrystalProp(const StageLoader::CrystalData& crystalData);
    ~CrystalProp() override = default;

    void ApplyStageData(const StageLoader::CrystalData& crystalData);
    void ApplyShaderParams(const ShaderParamList& params, const ShaderParamListWithMaterialName& materialParams);
    void SetBreakParticleSystem(ParticleSystem* particleSystem) { breakParticleSystem = particleSystem; }
    void SetDestroyedCallback(std::function<void(CrystalProp*)> callback) { destroyedCallback = std::move(callback); }
    void Update() override;
private:
    void SpawnBreakParticles();
    void OnDamaged(const DamageData& damageData) override;
    void OnDead(const DamageData& damageData) override;

    std::shared_ptr<Model> model;
    ShaderParamListWithMaterialName shaderParams = {};
    ParticleSystem* breakParticleSystem = nullptr;
    std::function<void(CrystalProp*)> destroyedCallback = {};
    Rigidbody* rigidbody = nullptr;
    MeshCollider* meshCollider = nullptr;
    ModelRenderComponent* modelRenderer = nullptr;
    DamageHoleComponent* damageHoleComponent = nullptr;
};



