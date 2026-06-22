// Apple.cpp
#if 0

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
}

void Apple::OnCollisionEnter(Collider* self, Collider* other, const Vector3& point, const Vector3& normal)
{
    Actor* otherActor = other->GetOwnerActor();
    if (otherActor->CompareTag("Player"))
    {
        if (isAggressive)
        {

        }
    }
}

void Apple::OnCollisionExit(Collider* self, Collider* other, const Vector3& point, const Vector3& normal)
{
}

void Apple::OnTriggerEnter(Collider* self, Collider* other, const Vector3& point, const Vector3& normal)
{
}

void Apple::OnTriggerExit(Collider* self, Collider* other, const Vector3& point, const Vector3& normal)
{
}

void Apple::OnDead()
{
    printf("Apple Dead!\n");
    Destroy();
}

#endif
