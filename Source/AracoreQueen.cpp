// AracoreQueen.cpp

#include "AracoreQueen.h"
#include "DamageHoleComponent.h"
#include "ResourceManager.h"
#include "HitEffect.h"

AracoreQueen::AracoreQueen() : Entity("AracoreQueen", "Enemy", true, Layer::Enemy, 1000.0f, 1000.0f)
{
    std::shared_ptr<Model> model =
        ResourceManager::Instance().LoadModel("Data/Model/turtle_tears_vending_machine.glb");

    transform.SetScale(1);
    transform.SetPosition(0, 20, 20);

    rb = AddComponent<RigidbodyDynamic>();

    model->UpdateTransform(transform.matrix);
    AddComponent<MeshCollider>(rb, model.get(), true, 1280, PhysicsManager::Instance().GetPhysics()->createMaterial(0.5f, 0.5f, 0));

	// モデルレンダラーとダメージホールコンポーネントを追加
    ModelRenderComponent* modelRenderer = AddComponent<ModelRenderComponent>(model, ModelShaderId::PBR);
    damageHoleComponent = AddComponent<DamageHoleComponent>(modelRenderer, 0.65f, 0.12f, 0.55f, 1.35f);
}

void AracoreQueen::OnUpdate()
{
    Entity::OnUpdate();
}

void AracoreQueen::OnDrawGUI()
{
    Entity::OnDrawGUI();
}

void AracoreQueen::OnDamaged(const DamageData& damageData)
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

void AracoreQueen::OnCollisionEnter(Collider* self, Collider* other, const Vector3& point, const Vector3& normal)
{
    // プレイヤーに攻撃する
    Actor* otherActor = other->GetOwnerActor();
    if (otherActor->CompareTag("Player"))
    {
        if (isAggressive)
        {
            static_cast<Entity*>(otherActor)->TakeDamage({
                .damage = 10.0f,
                .source = this,
                .hitPosition = transform.position,
                .knockBackPower = 20.0f
                });
        }
    }
}

void AracoreQueen::OnCollisionExit(Collider* self, Collider* other, const Vector3& point, const Vector3& normal)
{
}

void AracoreQueen::OnTriggerEnter(Collider* self, Collider* other, const Vector3& point, const Vector3& normal)
{
}

void AracoreQueen::OnTriggerExit(Collider* self, Collider* other, const Vector3& point, const Vector3& normal)
{
}

void AracoreQueen::OnDead()
{
    printf("AracoreQueen Dead!\n");
    Destroy();
}
