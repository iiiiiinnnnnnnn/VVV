// Aracore.cpp

#include "Aracore.h"
#include "DamageHoleComponent.h"
#include "ResourceManager.h"
#include "HitEffect.h"
#include "ActorManager.h"
#include "Prop.h"

Aracore::Aracore() : Entity("Aracore", "Enemy", true, Layer::Enemy, 1000.0f, 1000.0f)
{
	// 蜘蛛の部分
    {
        // モデル描画
        std::shared_ptr<Model> model =
            ResourceManager::Instance().LoadModel("Data/Model/Spider/animated_spider.glb");
        transform.Update();
        model->UpdateTransform(transform.matrix);
        AddComponent<ModelRenderComponent>(model, ModelShaderId::PBR);

        // アニメータ
		anim = AddComponent<Animator>(model, 0);
        anim->Load("Data/Animator/animated_spider.animator");
    }
}

void Aracore::OnRegistered(ActorManager* actorManager)
{
	// 機械の部分
    #if 1
    {
        std::shared_ptr<Model> model =
            ResourceManager::Instance().LoadModel("Data/Model/Prop/turtle_tears_vending_machine.glb");

        auto machineShared = std::make_shared<Prop>(model, transform, false, 1280);
        auto machine = machineShared.get();
        actorManager->Register(machineShared);

        std::vector<Model::NodePose> nodePoses;
        model->GetNodePoses(nodePoses);
        machine->transform.position = nodePoses[model->GetNodeIndex("Box01")].position;

        // モデルレンダラーとダメージホールコンポーネントを追加
        ModelRenderComponent* modelRenderer = machine->AddComponent<ModelRenderComponent>(model, ModelShaderId::PBR);
        damageHoleComponent = machine->AddComponent<DamageHoleComponent>(modelRenderer, 0.65f, 0.12f, 0.55f, 1.35f);
    }
    #endif
}

void Aracore::OnUpdate()
{
    Entity::OnUpdate();
}

void Aracore::OnDrawGUI()
{
    Entity::OnDrawGUI();
}

void Aracore::OnDamaged(const DamageData& damageData)
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

void Aracore::OnCollisionEnter(Actor* other)
{
    // プレイヤーに攻撃する
    if (other->CompareTag("Player"))
    {
        static_cast<Entity*>(other)->TakeDamage({
            .damage = 10.0f,
            .source = this,
            .hitPosition = other->transform.position,
            .knockBackPower = 20.0f
            });
    }
}

void Aracore::OnCollisionExit(Actor* other)
{
}

void Aracore::OnTriggerEnter(Actor* other)
{
    // プレイヤーから攻撃受ける
    if (other->CompareTag("Player"))
    {
        TakeDamage({.damage = 10.0f});
    }
}

void Aracore::OnTriggerExit(Actor* other)
{
}

void Aracore::OnDead()
{
    printf("Aracore Dead!\n");
    Destroy();
}
