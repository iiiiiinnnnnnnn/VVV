// AracoreQueen.cpp

#include "AracoreQueen.h"
#include "Animator.h"
#include "BoneFollower.h"
#include "BoxCollider.h"
#include "CharacterController.h"
#include "Model.h"
#include "ModelRenderComponent.h"
#include "PhysicsComponent.h"
#include "RigidBody.h"
#include "SphereCollider.h"
#include "DamageHoleComponent.h"
#include "ResourceManager.h"
#include "ActorManager.h"
#include "NavMeshAgent.h"
#include "NavMeshActor.h"
#include "PostProcessController.h"
#include "CameraEffectController.h"
#include "Easing.h"
#include "HitStop.h"
#include "SpiderFootIK.h"

AracoreQueen::AracoreQueen() : Entity("AracoreQueen", "Enemy", true, 100.0f, 100.0f)
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
                    {"occlusion", 1.0f},
                    {"occlusionStrength", 0.0f}
                }
            }
        };
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
        CharacterController* cc = AddComponent<CharacterController>(Layers::Get("Enemy"), 3.86f, 0.01f);
        cc->SetStepOffset(1.2f);
        cc->SetSlopeLimitDeg(70.0f);
        cc->SetContactOffset(0.2f);
        navAgentRadius = 3.86f;
        navMeshAgent = AddComponent<NavMeshAgent>();

        // リジッドボディ
        rb = AddComponent<RigidbodyDynamic>();
        rb->SetKinematic(true);

        // 当たり判定
        bodyCollider = AddComponent<SphereCollider>(Layers::Get("Enemy"), rb, 3.66f, Vector3{0, 3.55f, 0});

        // 足接地補正
        AddComponent<SpiderFootIK>(Layers::Get("Foot"), model.get(), anim);

        // 足の当たり判定
        #if 0
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
        for (const std::string& ikBoneName : ikBoneNames)
        {
            const int ikNodeIndex = model->GetNodeIndex(ikBoneName.c_str());
            // 足接触コライダー
            IKColliders.push_back(AddComponent<BoneCapsuleCollider>(
                model.get(),

                ikNodeIndex,
                1.26f,
                2.8f,
                Matrix::CreateFromYawPitchRoll(0.0f, RAD(90.0f), 0.0f) *
                Matrix::CreateTranslation(-7.41f, 15.0f, 10.0f),
                PhysicsManager::Instance().GetDefaultMaterial(),
                false));
        }
        #endif
    }
}

void AracoreQueen::OnRegistered(ActorManager* actorManager)
{
    auto make = std::static_pointer_cast<Actor>(std::make_shared<AracoreQueenMachine>(this));
    machine = make.get();
    actorManager->Register(std::move(make));
}

void AracoreQueen::OnUpdate()
{
    Entity::OnUpdate();
    UpdateChase();

    anim->SetFloat("speed", navMeshAgent->GetMoveAmount());
}

void AracoreQueen::UpdateChase()
{
    if (!navMeshAgent) return;

    if (NavMeshActor* navMeshActor = NavMeshActor::GetActive())
        navMeshActor->SetAgentRadius(navAgentRadius);

	Actor* player = Actor::FindActorByTag("Player");
    if (!player)
    {
        chasingPlayer = ChaseType::No;
        navMeshAgent->Stop();
        return;
    }

    const float distance = Vector3::Distance(player->transform.position, transform.position);
    if (distance < 15.0f)
    {
        if (chaisedTimer > 0.0f)
        {
			chaisedTimer -= Game::Time::deltaTime;
            if(chaisedTimer <= 0.0f)
            {
                chasingPlayer = ChaseType::No;
                chaisedTimer = 0.0f;
			}
        }
    }
    else
    {
        if (distance > 20.0f)
        {
            chasingPlayer = ChaseType::Run;
            navMeshAgent->SetSpeed(6.0f);
            chaisedTimer = 3.0f;
        }
        else
        {
            chasingPlayer = ChaseType::Walk;
            navMeshAgent->SetSpeed(3.0f);
            chaisedTimer = 3.0f;
        }
    }

    if (chasingPlayer != ChaseType::No)
    {
        navMeshAgent->MoveToTarget(player);
    }
    else
    {
        navMeshAgent->Stop();
    }
}

void AracoreQueen::OnDrawGUI()
{
    Entity::OnDrawGUI();

    if (!ImGui::TreeNode("AracoreQueen AI"))
        return;

    ImGui::TreePop();
}

void AracoreQueen::OnCollisionEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal)
{
    // 踏みつけ判定に当たったらプレイヤーにダメージ
    if (!IsDead() && anim->GetCurrentStateName(0) == "run")
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

        Actor* otherActor = other->GetOwnerAsActor();
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

void AracoreQueen::OnDead()
{
    printf("AracoreQueen Dead!\n");
    if (machine)
        machine->Destroy(3);
	anim->SetBool("Dead", true);
    //Destroy(5);
}

// AracoreQueenMachine(AracoreQueen.cpp)

AracoreQueenMachine::AracoreQueenMachine(AracoreQueen* ownerAracoreQueen)
    : Entity("AracoreQueenMachine", "Enemy", true, 100.0f, 100.0f),
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

    // Box02に追従
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
    damageHoleComponent = AddComponent<DamageHoleComponent>(modelRenderer, 0.85f, 0.18f, 0.9f, 1.35f);
}

void AracoreQueenMachine::OnDamaged(const DamageData& damageData)
{
    // ボコッ
    Actor* hitActor = damageData.hitColliderSelf ? damageData.hitColliderSelf->GetOwnerAsActor() : nullptr;
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

void AracoreQueenMachine::OnDead()
{
    printf("AracoreQueenMachine Dead!\n");
    Destroy(2);
}

