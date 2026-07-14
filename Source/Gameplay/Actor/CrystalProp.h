// CrystalProp.h

#pragma once

#include <functional>
#include <memory>

#include "Gameplay/Actor/Actor.h"
#include "Resource/Model.h"
#include "Physics/Core/PhysicsComponent.h"
#include "Rendering/Core/ShaderParam.h"
#include "Gameplay/Stage/Component/StageLoader.h"

class MeshCollider;
class ParticleSystem;
class PhysicsComponent;
class Rigidbody;

class CrystalProp : public Actor
{
public:
    CrystalProp(const StageLoader::CrystalData& crystalData);
    ~CrystalProp() override = default;

    void ApplyStageData(const StageLoader::CrystalData& crystalData);
    void ApplyShaderParams(const ShaderParamList& params, const ShaderParamListWithMaterialName& materialParams);
    void SetBreakParticleSystem(ParticleSystem* particleSystem) { breakParticleSystem = particleSystem; }
    void SetDestroyedCallback(std::function<void(CrystalProp*)> callback) { destroyedCallback = std::move(callback); }
    void Update() override;
    void Render(const RenderContext& rc) override;
    void OnCollisionEnter(
        PhysicsComponent* self,
        PhysicsComponent* other,
        const Vector3& point,
        const Vector3& normal) override;
    void OnTriggerEnter(
        PhysicsComponent* self,
        PhysicsComponent* other,
        const Vector3& point,
        const Vector3& normal) override;

private:
    void SpawnBreakParticles();
    bool IsBreakLayer(LayerId layerId) const;
    void TryBreak(PhysicsComponent* other);

    std::shared_ptr<Model> model;
    ShaderParamListWithMaterialName shaderParams = {};
    ParticleSystem* breakParticleSystem = nullptr;
    std::function<void(CrystalProp*)> destroyedCallback = {};
    Rigidbody* rigidbody = nullptr;
    MeshCollider* meshCollider = nullptr;
};



