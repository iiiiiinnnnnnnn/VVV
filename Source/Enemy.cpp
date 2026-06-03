// Enemy.cpp

#include "Enemy.h"
#include "ResourceManager.h"

Enemy::Enemy() : Actor("Enemy", "Enemy", "Default")
{
    std::shared_ptr<Model> model =
        ResourceManager::Instance().LoadModel("Data/Model/apple.glb");

    transform.SetScale(50.0f);
    model->UpdateTransform(transform.matrix);

    auto* rb = AddComponent<RigidbodyDynamic>();
    AddComponent<SphereCollider>(rb, 2.7f);
    rb->SetPosition({ 8.0f, 0.0f, 8.0f });

    // ƒ‚ƒfƒ‹ƒŒƒ“ƒ_ƒ‰[¶¬
    AddComponent<ModelRenderComponent>(
        model, ModelShaderId::PBR);
}

void Enemy::OnUpdate()
{
}

void Enemy::OnDrawGUI()
{
}
