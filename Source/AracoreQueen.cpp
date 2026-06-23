// AracoreQueen.cpp

#include "AracoreQueen.h"
#include "DamageHoleComponent.h"
#include "ResourceManager.h"
#include "SceneEffect.h"
#include "ActorManager.h"
#include "NavMeshAgent.h"
#include "AracoreFootGrounder.h"

AracoreQueen::AracoreQueen() : Entity("AracoreQueen", "Enemy", true, Layer::Enemy, 100.0f, 100.0f)
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
        anim->Load("Data/Animator/animated_spider_test.animator");
        anim->AddCallbackFunc("ThreatFunc",
            [this](const Animator::State& s)
        {
            // Enter
			CameraThreaten::Request(1.0f, 1.5f);
			ThreatenLines::Request(1.7f);
            CameraShake::Request(2.0f, 0.1f);
        },
            nullptr);
        anim->BindCallbacks();

        // キャラクターコントローラー
        CharacterController* cc = AddComponent<CharacterController>(2.17f, 0.7f);
        cc->SetStepOffset(1.2f);
        cc->SetSlopeLimitDeg(70.0f);
        cc->SetContactOffset(0.2f);
        navMeshAgent = AddComponent<NavMeshAgent>();

        // リジッドボディ
        rb = AddComponent<RigidbodyDynamic>();
        rb->SetKinematic(true);

        // 当たり判定
        bodyCollider = AddComponent<SphereCollider>(rb, 4.68f, Vector3{0, 3.55f, 0});

        // 足接地補正
        #if 0
        AracoreFootGrounder* footGrounder = AddComponent<AracoreFootGrounder>(model.get());
        footGrounder->AddLeg("Box09", "Box11");
        footGrounder->AddLeg("Box20", "Box19");
        footGrounder->AddLeg("Box25", "Box23");
        footGrounder->AddLeg("Box26", "Box24");
        footGrounder->AddLeg("Box31", "Box29");
        footGrounder->AddLeg("Box35", "Box36");
        footGrounder->AddLeg("Box37", "Box34");
        footGrounder->AddLeg("Box38", "Box30");
        #endif

        // 足の当たり判定
        #if 1
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
                1.2f,
                2.0f,
                Matrix::CreateFromYawPitchRoll(0.0f, RAD(90.0f), 0.0f) * Matrix::CreateTranslation(0.0f, 0.0f, 25.0f),
                PhysicsManager::Instance().GetDefaultMaterial(),
                false));

            // 踏みつけ激薄コライダー
            IKStampColliders.push_back(AddComponent<BoneBoxCollider>(
                model.get(),
                ikNodeIndex,
                Vector3(0.7f, 0.1f, 0.7f),
                Matrix::Identity,
                nullptr,
                true,
                false,
                true
            ));
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
    //UpdateChase();
}
Actor* AracoreQueen::FindPlayer() const
{
    ActorManager* actorManager = GetActorManager();
    if (!actorManager) return nullptr;

    for (const std::shared_ptr<Actor>& actor : actorManager->GetActors())
    {
        if (!actor || actor->IsPendingDestroy()) continue;
        if (!actor->CompareTag("Player")) continue;
        return actor.get();
    }

    return nullptr;
}

void AracoreQueen::UpdateChase()
{
    if (!navMeshAgent) return;

    Actor* player = FindPlayer();
    if (!player)
    {
        if (chasingPlayer)
            navMeshAgent->Stop();
        chasingPlayer = false;
        return;
    }

    Vector3 toPlayer = player->transform.position - transform.position;
    toPlayer.y = 0.0f;
    const float distanceSq = toPlayer.LengthSquared();
    const float startDistanceSq = chaseStartDistance * chaseStartDistance;
    const float stopDistanceSq = chaseStopDistance * chaseStopDistance;

    if (!chasingPlayer && distanceSq <= startDistanceSq)
        chasingPlayer = true;
    else if (chasingPlayer && distanceSq >= stopDistanceSq)
    {
        chasingPlayer = false;
        navMeshAgent->Stop();
    }

    if (chasingPlayer)
        navMeshAgent->MoveToTarget(player);
}
void AracoreQueen::OnDrawGUI()
{
    Entity::OnDrawGUI();

    if (!ImGui::TreeNode("AracoreQueen AI"))
        return;

    ImGui::Checkbox("Chasing Player", &chasingPlayer);
    ImGui::DragFloat("Chase Start Distance", &chaseStartDistance, 0.1f, 0.0f, 100.0f);
    ImGui::DragFloat("Chase Stop Distance", &chaseStopDistance, 0.1f, 0.0f, 100.0f);
    ImGui::TreePop();
}

void AracoreQueen::OnCollisionEnter(Collider* self, Collider* other, const Vector3& point, const Vector3& normal)
{

}

void AracoreQueen::OnTriggerEnter(Collider* self, Collider* other, const Vector3& point, const Vector3& normal)
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
                .knockBackPower = 10.0f,
                .hitColliderSelf = self,
                .hitColliderOther = other,
                .hitPosition = point,
                .hitNormal = normal
                });
        }
    }
}

void AracoreQueen::OnDamaged(const DamageData& damageData)
{
    HitStop::Request(0.15f);
    CameraShake::Request(0.2f, 0.1f);
}

void AracoreQueen::OnDead()
{
    printf("AracoreQueen Dead!\n");
    if (machine)
    {
        machine->Destroy();
    }
	anim->SetBool("Dead", true);
    Destroy(5);
}

// AracoreQueenMachine(AracoreQueen.cpp)

AracoreQueenMachine::AracoreQueenMachine(AracoreQueen* ownerAracoreQueen)
    : Entity("AracoreQueenMachine", "Enemy", true, Layer::EnemyM, 100.0f, 100.0f),
    ownerAracoreQueen(ownerAracoreQueen)
{
    std::shared_ptr<Model> model =
        ResourceManager::Instance().LoadModel("Data/Model/Prop/vending.glb");

    // リジッドボディ
    auto rb = AddComponent<RigidbodyDynamic>();
    rb->SetKinematic(true);

    // 当たり判定
    collider = AddComponent<BoxCollider>(
        rb, Vector3{7.33f * 0.5f, 11.69f * 0.5f, 5.0f * 0.5f}, Vector3{0.0f, 0.29f, 0.0f});

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
    Actor* hitActor = damageData.hitColliderSelf ? damageData.hitColliderSelf->GetOwnerActor() : nullptr;
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