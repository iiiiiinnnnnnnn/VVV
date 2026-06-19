// Aracore.cpp

#include "Aracore.h"
#include "DamageHoleComponent.h"
#include "ResourceManager.h"
#include "HitEffect.h"
#include "ActorManager.h"

Aracore::Aracore() : Entity("Aracore", "Enemy", true, Layer::Enemy, 1000.0f, 1000.0f)
{
	// 蜘蛛の部分
    {
		shaderParamWithMaterialName =
		{
			{
				"03 - Default",
			{
                {"metalness", 0.0f},
            {"roughness", 1.0f},
            {"occlusion", 0.5f},
            {"occlusionStrength", 1.0f}
		}
			}
		};

        // モデル描画
        std::shared_ptr<Model> model =
            ResourceManager::Instance().LoadModel("Data/Model/Spider/animated_spider.glb");
		transform.SetScale(0.04f);
        model->UpdateTransform(transform.matrix);
        bodyRenderer = AddComponent<ModelRenderComponent>(model, ModelShaderId::PBR, shaderParamWithMaterialName);

        // アニメータ
		anim = AddComponent<Animator>(model, 0);
        anim->Load("Data/Animator/animated_spider.animator");

		// キャラクターコントローラー
		AddComponent<CharacterController>(1.13f, 0.001f);

		// リジッドボディ
		rb = AddComponent<RigidbodyDynamic>();
		rb->SetKinematic(true);

        // 当たり判定
        AddComponent<SphereCollider>(rb, 4.68f, Vector3{0, 3.55f, 0});

        /*append bones
        IK Chain02
        IK Chain10
        IK Chain11
        IK Chain12
        IK Chain13
        IK Chain14
        IK Chain15
        IK Chain16
        */
        AddComponent<BoneSphereCollider>(
            model.get(),
            model->GetNodeIndex("IK Chain02"),
            0.8f,
			Matrix::CreateTranslation({-0.58f, 1.0f, 0.5f}));
    }
}

void Aracore::OnRegistered(ActorManager* actorManager)
{
	// 機械の部分
    {
        machineShaderParam =
        {
            {
                {"metalness", 1.0f},
            {"roughness", 0.0f},
            {"occlusion", 0.5f},
            {"occlusionStrength", 1.0f}
            }
        };
        std::shared_ptr<Model> model =
            ResourceManager::Instance().LoadModel("Data/Model/Prop/turtle_tears_vending_machine.glb");

        auto machineShared = std::make_shared<Actor>("Machine", "Enemy", true, Layer::Enemy);
        auto machine = machineShared.get();
        actorManager->Register(machineShared);

        // リジッドボディ
        auto rb = machine->AddComponent<RigidbodyDynamic>();
        rb->SetKinematic(true);

        // 当たり判定
        machine->AddComponent<BoxCollider>(rb, Vector3{3.72f, 2.95f, 6.14f}, Vector3{0.0f, 5.95f, 0.21f});

        // Box02に追従
        if (bodyRenderer)
        {
            Transform offset{};
            offset.SetPosition(0, -0.4f, 0);
            offset.SetScale(40.0f, 40.0f, 64.3f);
            machine->AddComponent<BoneFollower>(
                bodyRenderer->GetModel(), "Box02", offset);
        }

        // モデルレンダラーとダメージホールコンポーネントを追加
        ModelRenderComponent* modelRenderer = machine->AddComponent<ModelRenderComponent>(model, ModelShaderId::PBR);
		modelRenderer->SetShaderParamForAllMaterials(machineShaderParam);
        damageHoleComponent = machine->AddComponent<DamageHoleComponent>(modelRenderer, 0.65f, 0.12f, 0.55f, 1.35f);
    }
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
            .hitPosition = transform.position,
            .knockBackPower = 20.0f
            });
    }
}

void Aracore::OnCollisionExit(Actor* other)
{
}

void Aracore::OnTriggerEnter(Actor* other)
{
}

void Aracore::OnTriggerExit(Actor* other)
{
}

void Aracore::OnDead()
{
    printf("Aracore Dead!\n");
    Destroy();
}
