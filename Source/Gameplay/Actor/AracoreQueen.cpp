// AracoreQueen.cpp

#include "Gameplay/Actor/AracoreQueen.h"
#include "Animation/Animator.h"
#include "Animation/BoneFollower.h"
#include "Physics/Collider/BoxCollider.h"
#include "Physics/Collider/CharacterController.h"
#include "Resource/Model.h"
#include "Rendering/Component/ModelRenderComponent.h"
#include "Physics/Core/PhysicsComponent.h"
#include "Physics/Collider/SphereCollider.h"
#include "Rendering/Component/DamageHoleComponent.h"
#include "Resource/ResourceManager.h"
#include "Gameplay/Actor/ActorManager.h"
#include "Physics/Navigation/NavMeshAgent.h"
#include "Physics/Navigation/NavMeshActor.h"
#include "Gameplay/Scene/PostProcessController.h"
#include "Gameplay/Scene/CameraEffectController.h"
#include "Core/Foundation/Easing.h"
#include "Gameplay/Scene/HitStop.h"
#include "Animation/SpiderFootIK.h"
#include "Physics/Collider/BoneCapsuleCollider.h"
#include "Physics/Collider/BoneSphereCollider.h"

AracoreQueen::AracoreQueen() : Entity("AracoreQueen", "Enemy", true, 1000.0f, 1000.0f)
{
    // 蜘蛛の部分
    {
        // モデル
        model = ResourceManager::Instance().LoadModel("Data/Model/Spider/animated_spider.glb");
        shaderParamWithMaterialName =
        {
            {
                "03 - Default",
            {
                {"metalness", 0.0f},
            {"roughness", 1.0f},
            {"occlusion", 0.0f},
            {"occlusionStrength", 0.7f},
            {"emission", Color(0, 0, 0, 0)}
        }
            }
        };
        transform.SetPosition({-6, 3, 6});
        transform.SetScale(0.035f);
        model->UpdateTransform(transform.matrix);
        bodyRenderer = AddComponent<ModelRenderComponent>(model, ModelShaderId::PBR, shaderParamWithMaterialName);

        // アニメータ
        anim = AddComponent<Animator>(model, 0);
        anim->Load("Data/Animator/animated_spider.animator");
        anim->AddCallbackFunc("ThreatFunc",
            [this](const Animator::State& s)
        {
            PostProcessController::Instance().RequestThreaten(
                5.0f,
                3.0f,
                0.15f,
                Easing::Type::InSine,
                Easing::Type::OutCubic);
            CameraEffectController::Request(2.0f, 0.1f);
        },
            nullptr);
        anim->BindCallbacks();

        // キャラクターコントローラー
        CharacterController* cc = AddComponent<CharacterController>(
            Layers::Get("Enemy"), 3.0f, 0.01f);
        cc->SetStepOffset(1.2f);
        cc->SetSlopeLimitDeg(70.0f);
        cc->SetContactOffset(0.2f);
        navMeshAgent = AddComponent<NavMeshAgent>();

        // リジッドボディ
        rb = AddComponent<RigidbodyDynamic>();
        rb->SetKinematic(true);

        // 当たり判定
        bodyCollider = AddComponent<SphereCollider>(Layers::Get("Enemy"), rb, 3.66f, Vector3{0, 3.55f, 0});

        // 足接地補正
        spiderFootIK = AddComponent<SpiderFootIK>(Layers::Get("Foot"), model.get(), anim);
        spiderFootIK->SetWaistNodeIndex(model->GetNodeIndex("Dummy02"));
        spiderFootIK->AddLeg("Box09", "Box10", "Box11");
        spiderFootIK->AddLeg("Box20", "Box18", "Box19");
        spiderFootIK->AddLeg("Box25", "Box22", "Box23");
        spiderFootIK->AddLeg("Box26", "Box21", "Box24");
        spiderFootIK->AddLeg("Box31", "Box28", "Box29");
        spiderFootIK->AddLeg("Box35", "Box32", "Box36");
        spiderFootIK->AddLeg("Box37", "Box33", "Box34");
        spiderFootIK->AddLeg("Box38", "Box27", "Box30");

        // 足の当たり判定
        std::vector<std::string> ikBoneNames = {
            "IK Chain02",
            "IK Chain14",
            "IK Chain15",
            "IK Chain16",
            "IK Chain13",
            "IK Chain12",
            "IK Chain11",
            "IK Chain10"
        };
        colPositions = {
            {-6.41f, 13.75f, 0.0f},
            {-10.96f, 8.36f, 0.0f},
            {-6.67f, -5.5f, 0.0f},
            {-3.82f, -5.19f, 0.0f},
            {6.41f, 13.75f, 0.0f},
            {6.23f, 8.9f, 0.0f},
            {3.51f, -3.4f, 0.0f},
            {-0.51f, -3.08f, 0.0f}
        };
        for (int i = 0; i < ikBoneNames.size(); i++)
        {
            const int ikNodeIndex = model->GetNodeIndex(ikBoneNames[i].c_str());

            // 足接触コライダー
            IKColliders.push_back(AddComponent<BoneSphereCollider>(
                Layers::Get("Enemy"),
                model.get(),
                ikNodeIndex,
                1.0f,
                Matrix::CreateFromYawPitchRoll(0.0f, RAD(90.0f), 0.0f) *
                Matrix::CreateTranslation(colPositions[i]),
                PhysicsManager::Instance().GetDefaultMaterial(),
                false));
        }
    }

    controller = AddComponent<EnemyAIFlow>();
    controller->SetGraphPath("Data/AI/SpiderChase.json");
    if (!controller->Load(controller->GetGraphPath()))
        controller->CreateDefaultChaseGraph();
    controller->SetAgentRadius(3.0f);

    const auto stop = [this](const EnemyAIFlow::State&)
    {
        controller->StopMovement();
    };
    const auto walk = [this](const EnemyAIFlow::State&)
    {
        controller->MoveToTarget(3.0f);
    };
    const auto run = [this](const EnemyAIFlow::State&)
    {
        controller->MoveToTarget(6.0f);
    };
    controller->AddCallbackFunc("Idle", stop, stop, {}, stop);
    controller->AddCallbackFunc("Walk", walk, walk, {}, stop);
    controller->AddCallbackFunc("Run", run, run, {}, stop);
    controller->BindCallbacks();
}

void AracoreQueen::OnAwake()
{
    auto make = std::static_pointer_cast<Actor>(std::make_shared<AracoreQueenMachine>(this));
    machine = make.get();
    if (ActorManager* actorManager = ActorManager::GetActive())
        actorManager->Register(std::move(make));
}

void AracoreQueen::OnUpdate()
{
    Entity::OnUpdate();
    anim->SetFloat("speed", navMeshAgent->GetMoveAmount());
}

void AracoreQueen::OnDrawGUI()
{
    Entity::OnDrawGUI();

    if (!ImGui::TreeNode("AracoreQueen AI"))
        return;

    for(Vector3& pos : colPositions)
    {
		ImGui::PushID(&pos);
        ImGui::DragFloat3("Foot Collider Offset", &pos.x, 0.01f);
		ImGui::PopID();
	}

    ImGui::TreePop();
}

void AracoreQueen::OnCollisionEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal)
{
    if (IsDead()) return;

    // 踏みつけ判定に当たったらプレイヤーにダメージ
	std::string currentState = anim->GetCurrentStateName();
    if (currentState == "run" || currentState == "walk")
    {
        bool isFootCollider = false;
        for (PhysicsComponent* collider : IKColliders)
        {
            if (self == collider)
            {
                isFootCollider = true;
                break;
            }
        }
        if (!isFootCollider) return;

        Actor* otherActor = dynamic_cast<Actor*>(other->GetOwner());
        if (otherActor->CompareTag("Player"))
        {
            static_cast<Entity*>(otherActor)->TakeDamage({
                .damage = 10.0f,
                .knockBackPower = 10.0f,
                .hitColliderSelf = self,
                .hitColliderOther = other,
                .hitPosition = point,
                .hitNormal = normal
                });
        }
    }
}

void AracoreQueen::OnTriggerEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal)
{

}

void AracoreQueen::OnDamaged(const DamageData& damageData)
{
    HitStop::Request(0.15f);
    CameraEffectController::Request(0.2f, 0.1f);
}

void AracoreQueen::OnDead(const DamageData& damageData)
{
    if (controller) controller->SetActive(false);
    printf("AracoreQueen Dead!\n");
    if (machine)
        machine->Destroy(3);
	anim->SetBool("Dead", true);
    //Destroy(5);
}

// AracoreQueenMachine(AracoreQueen.cpp)

AracoreQueenMachine::AracoreQueenMachine(AracoreQueen* ownerAracoreQueen)
    : Entity("AracoreQueenMachine", "Enemy", true, ownerAracoreQueen->GetLife(), ownerAracoreQueen->GetMaxLife()),
    ownerAracoreQueen(ownerAracoreQueen)
{
    std::shared_ptr<Model> model =
        ResourceManager::Instance().LoadModel("Data/Model/Prop/vending.glb");

    // リジッドボディ
    auto rb = AddComponent<RigidbodyDynamic>();
    rb->SetKinematic(true);

    // 当たり判定
    collider = AddComponent<BoxCollider>(
        Layers::Get("EnemyAccessory"), rb, Vector3 { 3.665f, 5.85f, 2.5f }, Vector3{0.0f, 0.29f, 0.0f});

    // 親の体に追従
    Transform offset{};
    offset.SetPosition(0.0f, 43.8f, 9.6f);
    offset.SetAngle(-15.0f, 0.0f, 0.0f);
    offset.SetScale(40.3f, 34.9f, 47.9f);
    AddComponent<BoneFollower>(ownerAracoreQueen->model.get(), "Box02", offset);

    // モデルレンダラーとダメージホールコンポーネントを追加
    shaderParamWithMaterialName =
    {
        {
            "vend_main",
            {
                {"metalness", 0.2f},
                {"roughness", 0.0f},
                {"occlusion", 0.1f},
                {"occlusionStrength", 1.0f}
            }
        },
        {
            "venashi",
            {
                {"metalness", 1.0f},
                {"roughness", 0.0f},
                {"occlusion", 0.1f},
                {"occlusionStrength", 0.0f}
            }
        },
        {
            "vend_bottom",
            {
                {"metalness", 1.0f},
                {"roughness", 0.0f},
                {"occlusion", 0.1f},
                {"occlusionStrength", 0.0f}
            }
        },
        {
            "back_light",
            {
                {"metalness", 1.0f},
                {"roughness", 0.0f},
                {"occlusion", 0.1f},
                {"occlusionStrength", 0.0f}
            }
        },
        {
            "vend_main_toridashi",
            {
                {"metalness", 0.0f},
                {"roughness", 0.0f},
                {"occlusion", 0.1f},
                {"occlusionStrength", 0.0f}
            }
        },
        {
            "vend_glass2",
            {
                {"metalness", 0.0f},
                {"roughness", 0.0f},
                {"occlusion", 1.0f},
                {"occlusionStrength", 1.0f}
            }
        },
        {
            "vend_front_glass",
            {
                {"metalness", 0.5f},
                {"roughness", 0.0f},
                {"occlusion", 1.0f},
                {"occlusionStrength", 1.0f}
            }
        }
    };
    ModelRenderComponent* modelRenderer = AddComponent<ModelRenderComponent>(
        model, ModelShaderId::PBR, shaderParamWithMaterialName);
    damageHoleComponent = AddComponent<DamageHoleComponent>(modelRenderer, 3, 2, 2, 1);
}

void AracoreQueenMachine::OnDamaged(const DamageData& damageData)
{
    // ボコッ
    Actor* hitActor = damageData.hitColliderSelf ? dynamic_cast<Actor*>(damageData.hitColliderSelf->GetOwner()) : nullptr;
    if (damageData.hitColliderOther == collider && hitActor && hitActor->CompareTag("Player"))
    {
        if (damageData.hitPosition.has_value())
        {
            if (damageData.hitNormal.has_value())
                damageHoleComponent->AddDamageHoleFromPosition(damageData.hitPosition.value(), damageData.hitNormal.value());
            else
                damageHoleComponent->AddDamageHoleFromPosition(damageData.hitPosition.value());
            ownerAracoreQueen->TakeDamage(damageData);
        }
        else
        {
            damageHoleComponent->AddDamageHoleFrom(hitActor);
            ownerAracoreQueen->TakeDamage(damageData);
        }
    }
}

void AracoreQueenMachine::OnDead(const DamageData& damageData)
{
    printf("AracoreQueenMachine Dead!\n");
    Destroy(2);
}
