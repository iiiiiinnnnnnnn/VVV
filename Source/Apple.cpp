// Apple.cpp

#include "Apple.h"
#include "DamageHoleComponent.h"
#include "ResourceManager.h"
#include "HitEffect.h"

Apple::Apple() : Entity("Apple", "Enemy", true, Layer::Enemy, 1000.0f, 1000.0f)
{
    std::shared_ptr<Model> model =
        ResourceManager::Instance().LoadModel("Data/Model/apple.glb");

    transform.SetScale(50.0f);
    model->UpdateTransform(transform.matrix);

    rb = AddComponent<RigidbodyDynamic>();
    AddComponent<MeshCollider>(rb, model.get(), true, 1280, PhysicsManager::Instance().GetPhysics()->createMaterial(0.5f, 0.5f, 0));

    ModelRenderComponent* modelRenderer = AddComponent<ModelRenderComponent>(
        model, ModelShaderId::PBR);
    damageHoleComponent = AddComponent<DamageHoleComponent>(modelRenderer, 13.0f, 2.5f, 8.0f);
}

void Apple::SetPosition(const Vector3& pos)
{
    rb->SetPosition(pos);
}

void Apple::OnUpdate()
{
    Entity::OnUpdate();
}

void Apple::OnDrawGUI()
{
    Entity::OnDrawGUI();
}

void Apple::OnDamaged(const DamageData& damageData)
{
    HitStop::Request(0.15f);
    CameraShake::Request(0.2f, 0.1f);

    if (damageData.source)
    {
        const Entity* sourceEntity = damageData.source;
        damageHoleComponent->AddDamageHoleFrom(sourceEntity);
    }
}

void Apple::OnCollisionEnter(Actor* other)
{
    if (other->CompareTag("Player"))
    {
        if (isAggressive)
        {
            static_cast<Entity*>(other)->TakeDamage(
                {
                    .damage = 10.0f,
					.source = this,
                    .hitPosition = transform.position,
                    .knockBackPower = 20.0f
                });
        }
    }
}

void Apple::OnCollisionExit(Actor* other)
{
}

void Apple::OnTriggerEnter(Actor* other)
{
}

void Apple::OnTriggerExit(Actor* other)
{
}

void Apple::OnDead()
{
    printf("Apple Dead!\n");
    Destroy();
}
