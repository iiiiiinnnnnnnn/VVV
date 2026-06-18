// ZakoEnemyA.cpp

#include "ZakoEnemyA.h"
#include "ResourceManager.h"
#include "HitEffect.h"

ZakoEnemyA::ZakoEnemyA() : Entity("ZakoEnemyA", "Enemy", true, Layer::Enemy, 1000.0f, 1000.0f)
{
    std::shared_ptr<Model> model =
        ResourceManager::Instance().LoadModel("Data/Model/ZakoEnemyA.glb");

    transform.SetScale(50.0f);
    model->UpdateTransform(transform.matrix);

    rb = AddComponent<RigidbodyDynamic>();
    AddComponent<MeshCollider>(rb, model.get(), true, 1280, PhysicsManager::Instance().GetPhysics()->createMaterial(0.5f, 0.5f, 0));

    // ƒ‚ƒfƒ‹ƒŒƒ“ƒ_ƒ‰[¶¬
    AddComponent<ModelRenderComponent>(
        model, ModelShaderId::PBR);
}

void ZakoEnemyA::SetPosition(const Vector3& pos)
{
    rb->SetPosition(pos);
}

void ZakoEnemyA::OnUpdate()
{
    Entity::OnUpdate();
}

void ZakoEnemyA::OnDrawGUI()
{
    Entity::OnDrawGUI();
}

void ZakoEnemyA::OnDamaged(float damage, KnockBackData knockBackData)
{
    HitStop::Request(0.15f);
    CameraShake::Request(0.2f, 0.1f);
}

void ZakoEnemyA::OnCollisionEnter(Actor* other)
{
    if (other->CompareTag("Player"))
    {
        if (isAggressive)
        {
            static_cast<Entity*>(other)->TakeDamage(10.0f, {this, 20.0f});
        }
    }
}

void ZakoEnemyA::OnCollisionExit(Actor* other)
{
}

void ZakoEnemyA::OnTriggerEnter(Actor* other)
{
    if (other->CompareTag("Player"))
    {
        TakeDamage(10.0f);
    }
}

void ZakoEnemyA::OnTriggerExit(Actor* other)
{
}

void ZakoEnemyA::OnDead()
{
    printf("ZakoEnemyA Dead!\n");
    Destroy();
}
