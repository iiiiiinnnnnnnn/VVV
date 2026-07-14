// CrystalProp.cpp

#include "CrystalProp.h"

#include "CameraEffectController.h"
#include "Graphics.h"
#include "HitStop.h"
#include "MeshCollider.h"
#include "ModelRenderer.h"
#include "ParticleSystem.h"
#include "PhysicsComponent.h"
#include "ResourceManager.h"
#include "Rigidbody.h"

CrystalProp::CrystalProp(const StageLoader::CrystalData& crystalData)
    : Actor("CrystalProp", "CrystalProp", true)
{
    transform = crystalData.transform;
    transform.Update();

    model = ResourceManager::Instance().LoadModel("Data/Model/Prop/crystal.glb");
    rigidbody = AddComponent<RigidbodyStatic>();
    meshCollider = AddComponent<MeshCollider>(
        Layers::Get("Prop"),
        rigidbody,
        model.get(),
        transform.scale);
}

void CrystalProp::ApplyStageData(const StageLoader::CrystalData& crystalData)
{
    transform = crystalData.transform;
    transform.Update();
}

void CrystalProp::ApplyShaderParams(const ShaderParamList& params, const ShaderParamListWithMaterialName& materialParams)
{
    shaderParams = materialParams;
    ModelRenderer::SetShaderParamForAllMaterials(model.get(), params, shaderParams);
}

void CrystalProp::SpawnBreakParticles()
{
    if (!breakParticleSystem) return;

    const int particleCount = 32;
    for (int i = 0; i < particleCount; ++i)
    {
        Vector3 p = transform.position;
        p.x += Random::Range(-0.6f, 0.6f);
        p.y += Random::Range(+0.7f, 1.0f);
        p.z += Random::Range(-0.6f, 0.6f);

        Vector3 v;
        v.x = Random::Range(-1.75f, 1.75f);
        v.y = Random::Range(-0.45f, 1.05f);
        v.z = Random::Range(-1.75f, 1.75f);

        breakParticleSystem->Set(
            7,
            1.2f,
            p,
            v,
            Vector3(0.0f, -5.0f, 0.0f),
            Vector2(1.0f, 1.0f),
            false,
            24.0f,
            Color(0.35f, 0.9f, 1.0f, 1.0f));
    }
}

bool CrystalProp::IsBreakLayer(LayerId layerId) const
{
    return layerId == Layers::Get("Enemy") ||
        layerId == Layers::Get("PlayerAtk") ||
        layerId == Layers::Get("PlayerAttack") ||
        layerId == Layers::Get("EnemyAtk") ||
        layerId == Layers::Get("EnemyAttack");
}

void CrystalProp::TryBreak(PhysicsComponent* other)
{
    if (!other) return;
    if (!IsBreakLayer(other->GetLayerId())) return;

    HitStop::Request(0.06f);
    CameraEffectController::Request(0.1f, 0.06f);
    SpawnBreakParticles();
    Destroy();
    if (destroyedCallback) destroyedCallback(this);
}

void CrystalProp::Update()
{
    Actor::Update();
    model->UpdateTransform(transform.matrix);
    rigidbody->SetPosition(transform.position);
    rigidbody->SetRotation(transform.rotation);
    meshCollider->SetLocalScale(transform.scale);
}

void CrystalProp::Render(const RenderContext& rc)
{
    Game::Graphics::Instance().GetModelRenderer()->Draw(
        ModelShaderId::PBR,
        model,
        shaderParams);
}

void CrystalProp::OnCollisionEnter(
    PhysicsComponent* self,
    PhysicsComponent* other,
    const Vector3& point,
    const Vector3& normal)
{
    TryBreak(other);
}

void CrystalProp::OnTriggerEnter(
    PhysicsComponent* self,
    PhysicsComponent* other,
    const Vector3& point,
    const Vector3& normal)
{
    TryBreak(other);
}


