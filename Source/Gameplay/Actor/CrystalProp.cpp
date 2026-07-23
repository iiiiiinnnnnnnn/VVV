// CrystalProp.cpp

#include "Gameplay/Actor/CrystalProp.h"

#include "Gameplay/Scene/CameraEffectController.h"
#include "Rendering/Core/Graphics.h"
#include "Gameplay/Scene/HitStop.h"
#include "Physics/Collider/MeshCollider.h"
#include "Rendering/Renderer/ModelRenderer.h"
#include "Rendering/Component/VMDLModelComponent.h"
#include "Rendering/Component/DamageHoleComponent.h"
#include "Rendering/Effect/ParticleSystem.h"
#include "Physics/Core/PhysicsComponent.h"
#include "Resource/ResourceManager.h"
#include "Physics/RigidBody/Rigidbody.h"
#include "Physics/Navigation/NavMeshObstacle.h"

CrystalProp::CrystalProp(const StageLoader::CrystalData& crystalData)
    : Entity("CrystalProp", "CrystalProp", true)
{
    transform = crystalData.transform;
    transform.Update();

    float largestScale = (std::max)(fabsf(transform.scale.x), fabsf(transform.scale.y));
    largestScale = (std::max)(largestScale, fabsf(transform.scale.z));
    constexpr float baseScale = 0.5f;
    constexpr float lifePerScale = 87.0f;
    maxLife = ceilf((largestScale - baseScale) * lifePerScale);
    maxLife = (std::clamp)(maxLife, 1.0f, 100.0f);
    life = maxLife;

    model = ResourceManager::Instance().LoadModel("Data/Model/Prop/crystal");
    modelRenderer = AddComponent<VMDLModelComponent>(model, ModelShaderId::PBR, shaderParams);
    damageHoleComponent = AddComponent<DamageHoleComponent>(modelRenderer, 1, 1, 2, 0.1f);
    rigidbody = AddComponent<RigidbodyStatic>();
    meshCollider = AddComponent<MeshCollider>(
        Layers::Get("Prop"),
        rigidbody,
        model.get());
    navMeshObstacle = AddComponent<NavMeshObstacle>();
}

void CrystalProp::ApplyStageData(const StageLoader::CrystalData& crystalData)
{
    const Transform& nextTransform = crystalData.transform;
    const bool changed =
        (transform.position - nextTransform.position).LengthSquared() > eps ||
        (transform.scale - nextTransform.scale).LengthSquared() > eps ||
        fabsf(transform.rotation.x - nextTransform.rotation.x) > eps ||
        fabsf(transform.rotation.y - nextTransform.rotation.y) > eps ||
        fabsf(transform.rotation.z - nextTransform.rotation.z) > eps ||
        fabsf(transform.rotation.w - nextTransform.rotation.w) > eps;

    transform = crystalData.transform;
    transform.Update();
    if (changed && navMeshObstacle) navMeshObstacle->MarkDirty();
}

void CrystalProp::ApplyShaderParams(const ShaderParamList& params, const ShaderParamListWithMaterialName& materialParams)
{
    shaderParams = materialParams;
    ShaderParamListWithMaterialName& rendererParams = modelRenderer->GetParamsWithMaterial();
    rendererParams = shaderParams;
    ModelRenderer::SetShaderParamForAllMaterials(model.get(), params, rendererParams);
}

void CrystalProp::SpawnBreakParticles()
{
    if (!breakParticleSystem) return;

    float largestScale = (std::max)(fabsf(transform.scale.x), fabsf(transform.scale.y));
    largestScale = (std::max)(largestScale, fabsf(transform.scale.z));

    int particleCount = largestScale * 32 * 10;
    for (int i = 0; i < particleCount; ++i)
    {
        Vector3 p = transform.position;
        p.x += Random::Range(-0.6f, 0.6f);
        p.y += Random::Range(+0.7f, 1.0f);
        p.z += Random::Range(-0.6f, 0.6f);

        Vector3 v;
        v.x = Random::Range(-0.75f, 0.75f);
        v.y = Random::Range(+2.45f, 5.05f);
        v.z = Random::Range(-0.75f, 0.75f);

        breakParticleSystem->Set(
            7,
            5.2f,
            p,
            v,
            Vector3(0.0f, -5.0f, 0.0f),
            Vector2(0.2f, 0.2f),
            false,
            24.0f,
            Color(0.35f, 0.9f, 1.0f, 1.0f));
    }
}

void CrystalProp::OnDamaged(const DamageData& damageData)
{
    HitStop::Request(0.06f);
    CameraEffectController::Request(0.1f, 0.06f);

    if (damageData.hitPosition.has_value())
        damageHoleComponent->AddDamageHoleFromPosition(damageData.hitPosition.value(), damageData.hitNormal.value_or(Vector3::Zero));
}

void CrystalProp::OnDead(const DamageData& damageData)
{
    SpawnBreakParticles();
    Destroy();
    if (navMeshObstacle) navMeshObstacle->MarkDirty();
    if (destroyedCallback) destroyedCallback(this);
}

void CrystalProp::Update()
{
    Actor::Update();
    rigidbody->SetPosition(transform.position);
    rigidbody->SetRotation(transform.rotation);
}
