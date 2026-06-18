// BossEnemy.cpp

#include "BossEnemy.h"
#include "DamageHoleComponent.h"
#include "ResourceManager.h"
#include "HitEffect.h"

BossEnemy::BossEnemy() : Entity("BossEnemy", "Enemy", true, Layer::Enemy, 1000.0f, 1000.0f)
{
    std::shared_ptr<Model> model =
        ResourceManager::Instance().LoadModel("Data/Model/turtle_tears_vending_machine.glb");

    transform.SetScale(1);
    transform.SetPosition(0, 20, 20);

    rb = AddComponent<RigidbodyDynamic>();
    model->UpdateTransform(Matrix::CreateScale(transform.scale));
    AddComponent<MeshCollider>(rb, model.get(), true, 1280, PhysicsManager::Instance().GetPhysics()->createMaterial(0.5f, 0.5f, 0));
    model->UpdateTransform(transform.matrix);

    // ���f�������_���[����
    ModelRenderComponent* modelRenderer = AddComponent<ModelRenderComponent>(
        model, ModelShaderId::PBR);
    damageHoleComponent = AddComponent<DamageHoleComponent>(modelRenderer, 0.65f, 0.12f, 0.55f, 1.35f);
}

void BossEnemy::OnUpdate()
{
    Entity::OnUpdate();
}

void BossEnemy::OnDrawGUI()
{
    Entity::OnDrawGUI();
}

void BossEnemy::OnDamaged(const DamageData& damageData)
{
    HitStop::Request(0.15f);
    CameraShake::Request(0.2f, 0.1f);

    if (damageData.hitPosition.has_value())
    {
        damageHoleComponent->AddDamageHoleFromPosition(damageData.hitPosition.value());
    }
    else if (damageData.source)
    {
        damageHoleComponent->AddDamageHoleFrom(damageData.source);
    }
}

void BossEnemy::OnCollisionEnter(Actor* other)
{
    // プレイヤーに攻撃する
    if (other->CompareTag("Player"))
    {
        if (isAggressive)
        {
            static_cast<Entity*>(other)->TakeDamage({
                .damage = 10.0f,
                .source = this,
                .hitPosition = other->transform.position,
                .knockBackPower = 20.0f
                });
        }
    }
}

void BossEnemy::OnCollisionExit(Actor* other)
{
}

void BossEnemy::OnTriggerEnter(Actor* other)
{
    // プレイヤーから攻撃受ける
    if (other->CompareTag("Player"))
    {
        TakeDamage({.damage = 10.0f});
    }
}

void BossEnemy::OnTriggerExit(Actor* other)
{
}

void BossEnemy::OnDead()
{
    printf("BossEnemy Dead!\n");
    Destroy();
}




