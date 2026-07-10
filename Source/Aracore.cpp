// Aracore.cpp

#include "Aracore.h"
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
#include "BoneCapsuleCollider.h"
#include "BoneSphereCollider.h"

Aracore::Aracore() : Entity("Aracore", "Enemy", true, 1000.0f, 1000.0f)
{
    // 蜘蛛の部分
    {
        // モデル
        model = ResourceManager::Instance().LoadModel("Data/Model/Spider/animated_spider2.glb");
        shaderParamWithMaterialName =
        {
            {
                "Spider",
            {
                {"metalness", 0.0f},
            {"roughness", 1.0f},
            {"occlusion", 0.0f},
            {"occlusionStrength", 0.7f},
        {"emission", ColorFromRGBA(0x4AA5FFFF)}
        }
            },
            {
                "Yeux",
            {
                {"metalness", 0.0f},
            {"roughness", 0.1f},
            {"occlusion", 0.0f},
            {"occlusionStrength", 0.7f}
        }
            }
        };
        transform.SetPosition({-6, 3, 6});
        transform.SetScale(1);
        model->UpdateTransform(transform.matrix);
        bodyRenderer = AddComponent<ModelRenderComponent>(model, ModelShaderId::PBR, shaderParamWithMaterialName);

        // アニメータ
        anim = AddComponent<Animator>(model, 0);
        anim->Load("Data/Animator/animated_spider2.animator");
        anim->BindCallbacks();

        // キャラクターコントローラー
        CharacterController* cc = AddComponent<CharacterController>(
            Layers::Get("Enemy"), 0.4f, 0.01f);
        cc->SetStepOffset(1.2f);
        cc->SetSlopeLimitDeg(70.0f);
        cc->SetContactOffset(0.2f);
        navAgentRadius = 3.9f;
        navMeshAgent = AddComponent<NavMeshAgent>();

        // リジッドボディ
        rb = AddComponent<RigidbodyDynamic>();
        rb->SetKinematic(true);

        // 当たり判定
        bodyCollider = AddComponent<SphereCollider>(Layers::Get("Enemy"), rb, 0.6f, Vector3{0, 0.4f, 0});

        // 足接地補正
        spiderFootIK = AddComponent<SpiderFootIK>(Layers::Get("Foot"), model.get(), anim);
        spiderFootIK->SetRay(0.55f, 2.73f, 0.1f);
        spiderFootIK->SetModelVisualOffsetY(-0.2f);
        spiderFootIK->SetWaistNodeIndex(model->GetNodeIndex("Bone.004_012"));
        spiderFootIK->AddLeg("Bone_R.003_0105", "Bone_R.005_0107", "Bone_R.006_0108");
        spiderFootIK->AddLeg("Bone_L.003_0113", "Bone_L.005_0115", "Bone_L.006_0116");
        spiderFootIK->AddLeg("Bone.003_R.002_082", "Bone.003_R.004_084", "Bone.003_R.006_086");
        spiderFootIK->AddLeg("Bone.003_L.002_074", "Bone.003_L.004_076", "Bone.003_L.006_078");
        spiderFootIK->AddLeg("Bone.004_R.002_096","Bone.004_R.004_098","Bone.004_R.006_0100");
        spiderFootIK->AddLeg("Bone.004_L.002_00","Bone.004_L.004_090","Bone.004_L.006_092");
    }
}

void Aracore::OnRegistered(ActorManager* actorManager)
{
    auto make = std::static_pointer_cast<Actor>(std::make_shared<AracoreMachine>(this));
    machine = make.get();
    actorManager->Register(std::move(make));
}

void Aracore::OnUpdate()
{
    Entity::OnUpdate();
    UpdateChase();

    anim->SetFloat("speed", navMeshAgent->GetMoveAmount());
}

void Aracore::UpdateChase()
{
    if (!navMeshAgent) return;

    if (NavMeshActor* navMeshActor = NavMeshActor::GetActive())
    {
        navMeshActor->SetAgentRadius(navAgentRadius);
    }

    Actor* player = Actor::FindActorByTag("Player");
    if (!player)
    {
        chasingPlayer = ChaseType::No;
        navMeshAgent->Stop();
        return;
    }

    chaisedTimer -= Game::Time::deltaTime;
    const float distance = Vector3::Distance(
        player->transform.position, transform.position);
    if (distance < 15.0f)
    {
        if (chaisedTimer < 0.01f)
        {
            chasingPlayer = ChaseType::No;
            navMeshAgent->SetSpeed(3.0f);
            chaisedTimer = 2.0f;
        }
    }
    else
    {
        if (chaisedTimer < 0.01f)
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
    }

    if (chasingPlayer != ChaseType::No)
    {
        if (spiderFootIK && !spiderFootIK->HasGroundContact())
        {
            navMeshAgent->Stop();
            return;
        }

        navMeshAgent->MoveToTarget(player);
    }
    else
    {
        navMeshAgent->Stop();
    }
}

void Aracore::OnDrawGUI()
{
    Entity::OnDrawGUI();

    if (!ImGui::TreeNode("Aracore AI"))
        return;

    for (Vector3& pos : colPositions)
    {
        ImGui::PushID(&pos);
        ImGui::DragFloat3("Foot Collider Offset", &pos.x, 0.01f);
        ImGui::PopID();
    }

    ImGui::TreePop();
}

void Aracore::OnCollisionEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal)
{
    if (IsDead()) return;


}

void Aracore::OnTriggerEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal)
{

}

void Aracore::OnDamaged(const DamageData& damageData)
{
    HitStop::Request(0.15f);
    CameraEffectController::Request(0.2f, 0.1f);
}

void Aracore::OnDead()
{
    if (machine)
        machine->Destroy(0);
    anim->SetBool("Dead", true);
}

// AracoreMachine(Aracore.cpp)

AracoreMachine::AracoreMachine(Aracore* ownerAracore)
    : Entity("AracoreMachine", "Enemy", true, ownerAracore->GetLife(), ownerAracore->GetMaxLife()),
    ownerAracore(ownerAracore)
{
    std::shared_ptr<Model> model =
        ResourceManager::Instance().LoadModel("Data/Model/Prop/crystal.glb");

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
    AddComponent<BoneFollower>(ownerAracore->model.get(), "Box02", offset);

    // モデルレンダラーとダメージホールコンポーネントを追加
    shaderParamWithMaterialName = {};
    ModelRenderComponent* modelRenderer = AddComponent<ModelRenderComponent>(
        model, ModelShaderId::PBR, shaderParamWithMaterialName);
    damageHoleComponent = AddComponent<DamageHoleComponent>(modelRenderer, 3, 2, 2, 1);
}

void AracoreMachine::OnDamaged(const DamageData& damageData)
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
            ownerAracore->TakeDamage(damageData);
        }
        else
        {
            damageHoleComponent->AddDamageHoleFrom(hitActor);
            ownerAracore->TakeDamage(damageData);
        }
    }
}

void AracoreMachine::OnDead()
{
    printf("AracoreMachine Dead!\n");
    Destroy(2);
}



