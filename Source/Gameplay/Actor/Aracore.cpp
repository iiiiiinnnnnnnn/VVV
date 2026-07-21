// Aracore.cpp

#include "Gameplay/Actor/Aracore.h"
#include "Animation/Animator.h"
#include "Animation/BoneFollower.h"
#include "Physics/Collider/BoxCollider.h"
#include "Physics/Collider/CharacterController.h"
#include "Resource/Model.h"
#include "Rendering/Component/ModelRenderComponent.h"
#include "Physics/Core/PhysicsComponent.h"
#include "Physics/RigidBody/Rigidbody.h"
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

Aracore::Aracore(const Vector3& position) 
    : Entity("Aracore", "Enemy", true, 100.0f, 100.0f)
{
    // 蜘蛛の部分
    {
        // モデル
        model = ResourceManager::Instance().LoadModel("Data/Model/Spider/animated_spider2");
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
        transform.SetPosition(position);
        transform.SetScale(0.6f);
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

    controller = AddComponent<EnemyAIFlow>();
    controller->SetGraphPath("Data/AI/SpiderChase.json");
    if (!controller->Load(controller->GetGraphPath()))
        controller->CreateDefaultChaseGraph();
    controller->SetAgentRadius(2.0f);

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

void Aracore::OnAwake()
{
    auto make = std::static_pointer_cast<Actor>(std::make_shared<AracoreMachine>(this));
    machine = make.get();
    if (ActorManager* actorManager = ActorManager::GetActive())
        actorManager->Register(std::move(make));
}

void Aracore::OnUpdate()
{
    Entity::OnUpdate();
    anim->SetFloat("speed", navMeshAgent->GetMoveAmount());
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

void Aracore::OnDead(const DamageData& damageData)
{
    if (controller) controller->SetActive(false);
    if (machine)
    {
		SpawnBreakParticles();
        machine->Destroy(0);
        machine = nullptr;
    }
    anim->SetBool("Dead", true);
	bodyCollider->SetActive(false);
	rb->SetActive(false);
}

void Aracore::SpawnBreakParticles()
{
    if (!breakParticleSystem) return;

    const int particleCount = 16;
    for (int i = 0; i < particleCount; ++i)
    {
        Vector3 p = transform.position;
        p.x += Random::Range(-0.6f, 0.6f);
        p.y += Random::Range(+0.7f, 1.0f);
        p.z += Random::Range(-0.6f, 0.6f);

        Vector3 v;
        v.x = Random::Range(-1.75f, 1.75f);
        v.y = Random::Range(1.45f, 2.05f);
        v.z = Random::Range(-1.75f, 1.75f);

        breakParticleSystem->Set(
            7,
            1.2f,
            p,
            v,
            Vector3(0.0f, -5.0f, 0.0f),
            Vector2(0.2f, 0.2f),
            false,
            24.0f,
            Color(0.35f, 0.9f, 1.0f, 1.0f));
    }
}

// AracoreMachine(Aracore.cpp)

AracoreMachine::AracoreMachine(Aracore* ownerAracore)
    : Entity("AracoreMachine", "Enemy", true, ownerAracore->GetLife(), ownerAracore->GetMaxLife()),
    ownerAracore(ownerAracore)
{
    std::shared_ptr<Model> model =
        ResourceManager::Instance().LoadModel("Data/Model/Prop/crystal");

    // リジッドボディ
    auto rb = AddComponent<RigidbodyDynamic>();
    rb->SetKinematic(true);

    // 当たり判定
    collider = AddComponent<BoxCollider>(
        Layers::Get("EnemyAccessory"), rb, 
        Vector3 { 0.575f, 0.99f, 0.65f }, Vector3{0.0f, 1.0f, 0.0f});

    // 親の体に追従
    Transform offset{};
    offset.SetPosition(0.0f, 0.0f, 0.0f);
    offset.SetAngle(-90.0f, 0.0f, 13.9f);
    offset.SetScale(1.1f, 0.5f, 0.9f);
    AddComponent<BoneFollower>(ownerAracore->model.get(),
        "Bone.004_012", offset);

    // モデルレンダラーとダメージホールコンポーネントを追加
    shaderParamWithMaterialName = {};
    ModelRenderComponent* modelRenderer = AddComponent<ModelRenderComponent>(
        model, ModelShaderId::PBR, shaderParamWithMaterialName);
    damageHoleComponent = AddComponent<DamageHoleComponent>(
        modelRenderer, 0.5f, 0.5f, 1.2f, 0.1f);
}

void AracoreMachine::OnDamaged(const DamageData& damageData)
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
            ownerAracore->TakeDamage(damageData);
        }
        else
        {
            damageHoleComponent->AddDamageHoleFrom(hitActor);
            ownerAracore->TakeDamage(damageData);
        }
    }
}

void AracoreMachine::OnDead(const DamageData& damageData)
{
    if (ownerAracore)
        ownerAracore->SpawnBreakParticles();
	DamageData absoluteryDIED = damageData;
    absoluteryDIED.damage = 999999;
    ownerAracore->TakeDamage(absoluteryDIED);
    Destroy(0);
}
