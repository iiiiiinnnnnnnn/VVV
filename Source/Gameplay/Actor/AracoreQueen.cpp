// AracoreQueen.cpp

#include "Gameplay/Actor/AracoreQueen.h"
#include "Animation/Animator.h"
#include "Physics/Collider/CharacterController.h"
#include "Resource/VMDLModel.h"
#include "Rendering/Component/VMDL.h"
#include "Rendering/Component/VMDLModelComponent.h"
#include "Physics/Core/PhysicsComponent.h"
#include "Physics/Collider/CapsuleCollider.h"
#include "Physics/Collider/SphereCollider.h"
#include "Physics/Navigation/NavMeshAgent.h"
#include "Gameplay/Scene/PostProcessController.h"
#include "Gameplay/Scene/CameraEffectController.h"
#include "Core/Foundation/Easing.h"
#include "Gameplay/Scene/HitStop.h"
#include "Animation/MultiLegFootIK.h"

AracoreQueen::AracoreQueen(Vector3 position) : Entity("AracoreQueen", "Enemy", true, 1000.0f, 1000.0f)
{
    // 蜘蛛の部分
    {
        // モデル
        vmdl = AddComponent<VMDL>("Data/Model/Enemy/aracore");
        model = vmdl->GetSharedModel();
        transform.SetPosition(position);
        transform.SetScale(0.035f);
        model->UpdateTransform(transform.matrix);

        // アニメータ
        anim = vmdl->GetAnimator();
        anim->SetRootMotion("Box01");
        anim->Load("Data/Animator/animated_spider.animator");
        anim->AddCallbackFunc("ThreatFunc", [this](const Animator::State& s) { Threat(); }, nullptr);
        anim->BindCallbacks();

        // キャラクターコントローラー
        CharacterController* cc = AddComponent<CharacterController>(
            Layers::Get("Foot"), 3.0f, 0.01f);
        cc->SetPushable(false);
        cc->SetStepOffset(0.0f);
        cc->SetConstrainedClimbing(true);
        cc->SetSlopeLimitDeg(70.0f);
        cc->SetContactOffset(0.2f);
        navMeshAgent = AddComponent<NavMeshAgent>();

        multiLegFootIK = vmdl->GetMultiLegFootIK();
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
    const auto finding = [this](const EnemyAIFlow::State&)
    {
        if (isFinding)
        {
            controller->MoveToTarget(3.0f);

            if (findingTimer > 10.0f)
            {
                isFinding = false;
                findingTimer = 0.0f;
            }
			findingTimer += Game::Time::deltaTime;
        }
        else
        {
            controller->StopMovement();

            if (findingTimer > 6.0f)
            {
                isFinding = true;
                findingTimer = 0.0f;
            }
            findingTimer += Game::Time::deltaTime;
        }
    };
    const auto attack = [this](const EnemyAIFlow::State&)
    {
		anim->SetTrigger("Attack");
	};
    controller->AddCallbackFunc("Finding", finding, finding, {}, stop);
    controller->AddCallbackFunc("Attack", attack, attack, {}, stop);
    controller->BindCallbacks();
}

void AracoreQueen::Threat()
{
	anim->SetTrigger("Threat");
    PostProcessController::Instance().RequestThreaten(
        5.0f,
        3.0f,
        0.15f,
        Easing::Type::InSine,
        Easing::Type::OutCubic);
    CameraEffectController::Request(2.0f, 0.1f);
}

void AracoreQueen::OnUpdate()
{
    Entity::OnUpdate();
    anim->SetFloat("speed", navMeshAgent->GetMoveAmount());
}

void AracoreQueen::OnDrawGUI()
{
    Entity::OnDrawGUI();

    if (ImGui::Button("THREAT"))
    {
        Threat();
    }

    for (Vector3& pos : colPositions)
    {
        ImGui::PushID(&pos);
        ImGui::DragFloat3("Foot Collider Offset", &pos.x, 0.01f);
        ImGui::PopID();
    }
}

void AracoreQueen::OnCollisionEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal)
{
    if (IsDead()) return;
	PushPlayer(self, other);

    Actor* otherActor = dynamic_cast<Actor*>(other->GetOwner());

    if (self->GetLayerId() == Layers::Get("EnemyAtk"))
    {
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
        return;
    }

    // 踏みつけ判定に当たったらプレイヤーにダメージ
    if (self->GetLayerId() == Layers::Get("AracoreAtkStamp"))
    {
        std::string currentState = anim->GetCurrentStateName();
        if (currentState == "run" || currentState == "jump")
        {
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
}

void AracoreQueen::OnCollisionStay(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal)
{
	if (IsDead()) return;
	PushPlayer(self, other);
}

bool AracoreQueen::PushPlayer(PhysicsComponent* self, PhysicsComponent* other)
{
	if (self->CompareName("Enemy") || !other) return false;

	Entity* player = dynamic_cast<Entity*>(other->GetOwner());
	if (!player || !player->CompareTag("Player")) return false;

	Vector3 direction = player->transform.position - transform.position;
	direction.y = 0.0f;
	if (direction.LengthSquared() <= eps) direction = transform.forward;
	direction.Normalize();

	constexpr float pushSpeed = 10.0f;
	const float currentSpeed = player->GetKnockBackVelocity().Dot(direction);
	if (currentSpeed < pushSpeed)
		player->AddKnockBack(direction * (pushSpeed - currentSpeed));
	return true;
}

void AracoreQueen::OnTriggerEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal)
{
    if (IsDead()) return;
    PushPlayer(self, other);

    Actor* otherActor = dynamic_cast<Actor*>(other->GetOwner());

    if (self->GetLayerId() == Layers::Get("EnemyAtk"))
    {
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
        return;
    }

    // 踏みつけ判定に当たったらプレイヤーにダメージ
    if (self->GetLayerId() == Layers::Get("AracoreAtkStamp"))
    {
        std::string currentState = anim->GetCurrentStateName();
        if (currentState == "run" || currentState == "jump")
        {
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
	anim->SetBool("Dead", true);
    //Destroy(5);
}
