// Aracore.cpp

#include "Aracore.h"
#include "DamageHoleComponent.h"
#include "ResourceManager.h"
#include "HitEffect.h"
#include "ActorManager.h"
#include "NavMeshAgent.h"
#include "AracoreFootGrounder.h"


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

        // 足接地補正
        AracoreFootGrounder* footGrounder = AddComponent<AracoreFootGrounder>(model.get());

        // アニメータ
		anim = AddComponent<Animator>(model, 0);
        anim->Load("Data/Animator/animated_spider.animator");

		// キャラクターコントローラー
		CharacterController* cc = AddComponent<CharacterController>(0.45f, 0.7f);
		cc->SetStepOffset(1.2f);
		cc->SetSlopeLimitDeg(70.0f);
		cc->SetContactOffset(0.2f);
		AddComponent<NavMeshAgent>();

		// リジッドボディ
		rb = AddComponent<RigidbodyDynamic>();
		rb->SetKinematic(true);

        // 当たり判定
        //bodyCollider = AddComponent<SphereCollider>(rb, 4.68f, Vector3{0, 3.55f, 0});

        // 足接地補正対象。IK Chainではなくスキニングに効く足ボーンを補正する。
        footGrounder->AddLeg("Box09", "Box11");
        footGrounder->AddLeg("Box20", "Box19");
        footGrounder->AddLeg("Box25", "Box23");
        footGrounder->AddLeg("Box26", "Box24");
        footGrounder->AddLeg("Box31", "Box29");
        footGrounder->AddLeg("Box35", "Box36");
        footGrounder->AddLeg("Box37", "Box34");
        footGrounder->AddLeg("Box38", "Box30");

        // 足の当たり判定
        std::vector<std::string> ikBoneNames = {
            "IK Chain01",
            "IK Chain03",
            "IK Chain04",
            "IK Chain05",
            "IK Chain06",
            "IK Chain07",
            "IK Chain08",
            "IK Chain09"
		};
        for (const std::string& ikBoneName : ikBoneNames)
        {
            const int ikNodeIndex = model->GetNodeIndex(ikBoneName.c_str());
            // 足接触コライダー
            IKColliders.push_back(AddComponent<BoneCapsuleCollider>(
                model.get(),
                ikNodeIndex,
                1.42f,
                2.0f,
                Matrix::CreateFromYawPitchRoll(0.0f, RAD(90.0f), 0.0f) * Matrix::CreateTranslation(0.0f, 0.0f, 25.0f),
                PhysicsManager::Instance().GetDefaultMaterial(),
                false));

            // 踏みつけ激薄コライダー
            IKStampColliders.push_back(AddComponent<BoneBoxCollider>(
                model.get(),
                ikNodeIndex,
                Vector3{0.7f, 0.1f, 0.7f}));
        }
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

    for (Collider* collider : IKStampColliders)
    {
        BoneCollider* stampCollider = dynamic_cast<BoneCollider*>(collider);
        if (!stampCollider) continue;

        Actor* playerActor = stampCollider->FindOverlapActorByTag("Player");
        if (!playerActor) continue;

        static_cast<Entity*>(playerActor)->TakeDamage({
            .damage = 10.0f,
            .source = this,
            .hitPosition = stampCollider->GetWorldPosition(),
            .knockBackPower = 10.0f
            });
    }
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
        //damageHoleComponent->AddDamageHoleFromPosition(damageData.hitPosition.value());
    }
    else if (damageData.source)
    {
        //damageHoleComponent->AddDamageHoleFrom(damageData.source);
    }
}

void Aracore::OnCollisionEnter(Collider* self, Collider* other, const Vector3& point, const Vector3& normal)
{

}

void Aracore::OnCollisionExit(Collider* self, Collider* other, const Vector3& point, const Vector3& normal)
{
}

void Aracore::OnTriggerEnter(Collider* self, Collider* other, const Vector3& point, const Vector3& normal)
{
    // 踏みつけ判定に当たったらプレイヤーにダメージ
    {
        bool isFootCollider = false;
        for (Collider* collider : IKStampColliders)
        {
            if (self == collider)
            {
                isFootCollider = true;
                break;
            }
        }
        if (!isFootCollider) return;

        Actor* otherActor = other->GetOwnerActor();
        if (otherActor->CompareTag("Player"))
        {
            static_cast<Entity*>(otherActor)->TakeDamage({
                .damage = 10.0f,
                .source = this,
                .hitPosition = point,
                .knockBackPower = 10.0f
                });
        }
    }
}

void Aracore::OnTriggerExit(Collider* self, Collider* other, const Vector3& point, const Vector3& normal)
{
}

void Aracore::OnDead()
{
    printf("Aracore Dead!\n");
    Destroy(10);
}










