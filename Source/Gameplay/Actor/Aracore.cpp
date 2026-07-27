// Aracore.cpp

#include "Gameplay/Actor/Aracore.h"
#include <algorithm>
#include "Animation/Animator.h"
#include "Animation/BoneFollower.h"
#include "Physics/Collider/CharacterController.h"
#include "Resource/VMDLModel.h"
#include "Rendering/Component/VMDLModelComponent.h"
#include "Physics/Core/PhysicsComponent.h"
#include "Physics/RigidBody/Rigidbody.h"
#include "Physics/Collider/SphereCollider.h"
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
#include "Application/Time/GameTime.h"

Aracore::Aracore(const Vector3& position) 
    : Entity("Aracore", "Enemy", true, 100.0f, 100.0f)
{
    // 蜘蛛の部分
    {
        // モデル
        model = ResourceManager::Instance().LoadModel("Data/Model/Spider/animated_spider2");
        renderParams.materials =
        {
			{
				"Spider",
				{
					.emissionColor = ColorFromRGBA(0x4AA5FFFF),
					.metalness = 0.0f,
					.roughness = 1.0f,
					.occlusion = 0.0f,
					.occlusionStrength = 0.7f,
				}
			},
			{
				"Yeux",
				{
					.metalness = 0.0f,
					.roughness = 0.1f,
					.occlusion = 0.0f,
					.occlusionStrength = 0.7f,
				}
			},
        };
        transform.SetPosition(position);
        transform.SetScale(0.6f);
        model->UpdateTransform(transform.matrix);
        bodyRenderer = AddComponent<VMDLModelComponent>(model, ModelShaderId::VMat, renderParams);

        // アニメータ
        anim = AddComponent<Animator>(model, 0);
        anim->Load("Data/Animator/animated_spider2.animator");
        anim->AddCallbackFunc(
            "AttackHit",
            [this](const Animator::State&) { OnEnterAttackHit(); },
            [this](const Animator::State&) { OnExitAttackHit(); });
        anim->BindCallbacks();

        // キャラクターコントローラー
        characterController = AddComponent<CharacterController>(
            Layers::Get("Enemy"), 0.4f, 0.01f);
        characterController->SetStepOffset(1.2f);
        characterController->SetSlopeLimitDeg(70.0f);
        characterController->SetContactOffset(0.2f);
        navMeshAgent = AddComponent<NavMeshAgent>();

        // リジッドボディ
        rb = AddComponent<RigidbodyDynamic>();
        rb->SetKinematic(true);

        // 当たり判定
        bodyCollider = AddComponent<SphereCollider>(Layers::Get("Enemy"), rb, 0.6f, Vector3{0, 0.4f, 0});
        attackCollider = AddComponent<BoneSphereCollider>(
            Layers::Get("EnemyAtk"),
            model.get(),
            model->GetNodeIndex("Bone.004_012"),
            0.4f,
            Matrix::CreateTranslation(0.0f, 0.0f, 1.25f));
        attackCollider->SetActive(false);

        // 足接地補正
        spiderFootIK = AddComponent<SpiderFootIK>(Layers::Get("Foot"), model.get(), anim);
	spiderFootIK->SetRay(0.55f, 2.73f, 0.1f);
	spiderFootIK->SetModelVisualOffsetY(-0.2f);
	if (spiderFootIK->AddLegsFromVmdlSettings() == 0)
	{
		spiderFootIK->SetWaistNodeIndex(model->GetNodeIndex("Bone.004_012"));
		spiderFootIK->AddLeg("Bone_R.003_0105", "Bone_R.005_0107", "Bone_R.006_0108");
		spiderFootIK->AddLeg("Bone_L.003_0113", "Bone_L.005_0115", "Bone_L.006_0116");
		spiderFootIK->AddLeg("Bone.003_R.002_082", "Bone.003_R.004_084", "Bone.003_R.006_086");
		spiderFootIK->AddLeg("Bone.003_L.002_074", "Bone.003_L.004_076", "Bone.003_L.006_078");
		spiderFootIK->AddLeg("Bone.004_R.002_096", "Bone.004_R.004_098", "Bone.004_R.006_0100");
		spiderFootIK->AddLeg("Bone.004_L.002_00", "Bone.004_L.004_090", "Bone.004_L.006_092");
	}
    }

    controller = AddComponent<EnemyAIFlow>();
    controller->SetGraphPath("Data/AI/SpiderChase.json");
    if (!controller->Load(controller->GetGraphPath()))
        controller->CreateDefaultChaseGraph();
    controller->SetAgentRadius(1.5f);

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

void Aracore::TakeDamage(const DamageData& damageData)
{
	if (!IsDead()) return Entity::TakeDamage(damageData);
	if (!deathSequenceActive) return;

	HitStop::Request(0.15f);
	CameraEffectController::Request(0.2f, 0.1f);
	FinishDeathSequence();
}

void Aracore::Destroy(float delay)
{
	if (delay > 0.0f)
	{
		if (machine) machine->Destroy(delay);
		Object::Destroy(delay);
		return;
	}

	if (machine)
	{
		machine->Destroy(delay);
		machine = nullptr;
	}
	Actor::Destroy(delay);
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
	if (deathSequenceActive)
	{
		if (IsPendingDestroy()) FinishDeathSequence();
		return;
	}

    attackCooldown = std::max(attackCooldown - Game::Time::deltaTime, 0.0f);
    if (attackRequested)
    {
        attackRequestTimer += Game::Time::deltaTime;
        const bool inAttackState = anim->GetCurrentStateName() == "Attack";
        if (inAttackState) attackStateEntered = true;
        if ((attackStateEntered && !inAttackState) || attackRequestTimer >= 3.0f)
            EndAttack();
    }
    else if (controller && controller->GetTarget() && attackCooldown <= 0.0f)
    {
        const float distance = Vector3::Distance(
            transform.position, controller->GetTarget()->transform.position);
        if (distance <= 1.75f) BeginAttack();
    }

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
	if (self != attackCollider || !attackCollider->IsActive()) return;
	TryDealAttackDamage(dynamic_cast<Actor*>(other->GetOwner()), other, point, normal);
}

void Aracore::OnTriggerStay(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal)
{
	if (self != attackCollider || !attackCollider->IsActive()) return;
	TryDealAttackDamage(dynamic_cast<Actor*>(other->GetOwner()), other, point, normal);
}

void Aracore::OnDamaged(const DamageData& damageData)
{
    HitStop::Request(0.15f);
    CameraEffectController::Request(0.2f, 0.1f);

	Actor* attacker = damageData.hitColliderSelf
		? dynamic_cast<Actor*>(damageData.hitColliderSelf->GetOwner())
		: nullptr;
	if (controller && attacker && attacker->CompareTag("Player"))
		controller->LockOn(attacker);
}

void Aracore::OnDead(const DamageData& damageData)
{
    EndAttack();
    if (controller) controller->SetActive(false);
	if (navMeshAgent) navMeshAgent->SetActive(false);
	if (characterController)
	{
		characterController->ReleaseController();
		characterController->SetActive(false);
	}
	if (spiderFootIK) spiderFootIK->SetActive(false);
    if (machine)
    {
        machine->Destroy(0);
        machine = nullptr;
    }
    anim->SetBool("Dead", true);
	anim->SetFloat("speed", 0.0f);
	if (bodyCollider) bodyCollider->SetActive(true);
	if (rb)
	{
		rb->SetActive(true);
		rb->SetKinematic(false);

		Vector3 launchDirection = Vector3::Zero;
		if (damageData.hitPosition.has_value())
		{
			launchDirection = transform.position - damageData.hitPosition.value();
			launchDirection.y = 0.0f;
			if (launchDirection.LengthSquared() > eps) launchDirection.Normalize();
		}
		rb->SetVelocity(launchDirection * 2.5f + Vector3(0.0f, 1.5f, 0.0f));
		rb->SetAngularVelocity(Vector3(
			Random::Range(-5.0f, 5.0f),
			Random::Range(-3.0f, 3.0f),
			Random::Range(-5.0f, 5.0f)));
	}
	deathSequenceActive = true;
	Destroy(5.0f);
    SpawnBreakCrystalParticles();
}

void Aracore::FinishDeathSequence()
{
	if (!deathSequenceActive) return;

	deathSequenceActive = false;
	SpawnBreakSpiderParticles();
	Destroy();
}

void Aracore::BeginAttack()
{
	if (attackRequested || IsDead()) return;

	attackRequested = true;
	attackStateEntered = false;
	attackHit = false;
	attackRequestTimer = 0.0f;
	if (controller) controller->SetMovementLocked(true);
	anim->SetFloat("speed", 0.0f);
	anim->SetTrigger("Attack");
}

void Aracore::EndAttack()
{
	attackRequested = false;
	attackStateEntered = false;
	attackRequestTimer = 0.0f;
	attackCooldown = 0.8f;
	if (attackCollider) attackCollider->SetActive(false);
	if (controller) controller->SetMovementLocked(false);
}

void Aracore::OnEnterAttackHit()
{
	if (!attackRequested || IsDead()) return;

	attackHit = false;
	attackCollider->SetActive(true);
	Actor* actor = attackCollider->FindOverlapActorByTag("Player");
	if (!actor) return;

	Vector3 normal = actor->transform.position - attackCollider->GetWorldPosition();
	if (normal.LengthSquared() > eps) normal.Normalize();
	TryDealAttackDamage(actor, nullptr, actor->transform.position, normal);
}

void Aracore::OnExitAttackHit()
{
	if (attackCollider) attackCollider->SetActive(false);
	if (!IsDead()) SpawnAttackRangeParticles();
}

void Aracore::TryDealAttackDamage(
	Actor* actor,
	PhysicsComponent* otherCollider,
	const Vector3& point,
	const Vector3& normal)
{
	if (attackHit || !actor || !actor->CompareTag("Player")) return;

	Entity* entity = dynamic_cast<Entity*>(actor);
	if (!entity) return;

	attackHit = true;
	entity->TakeDamage({
		.damage = Random::Range(12.0f, 18.0f),
		.knockBackPower = 12.0f,
		.hitColliderSelf = attackCollider,
		.hitColliderOther = otherCollider,
		.hitPosition = point,
		.hitNormal = normal,
	});
}

void Aracore::SpawnAttackRangeParticles()
{
	if (!breakParticleSystem || !attackCollider) return;

	Vector3 center = attackCollider->GetWorldPosition();
	center.y = transform.position.y + 0.06f;
	constexpr float attackRadius = 0.95f;
	constexpr int boundaryParticleCount = 18;
	constexpr int innerParticleCount = 14;
	constexpr float pi2 = DirectX::XM_2PI;

	for (int i = 0; i < boundaryParticleCount; ++i)
	{
		const float angle = pi2 * static_cast<float>(i) /
			static_cast<float>(boundaryParticleCount);
		Vector3 position = center + Vector3(
			cosf(angle) * attackRadius,
			Random::Range(0.0f, 0.04f),
			sinf(angle) * attackRadius);
		breakParticleSystem->Set(
			7,
			0.5f,
			position,
			Vector3::Zero,
			Vector3::Zero,
			Vector2(0.18f, 0.18f),
			false,
			24.0f,
			Color(0.25f, 0.85f, 1.0f, 0.9f));
	}

	for (int i = 0; i < innerParticleCount; ++i)
	{
		const float ratio = sqrtf(
			(static_cast<float>(i) + 0.5f) /
			static_cast<float>(innerParticleCount));
		const float angle = static_cast<float>(i) * 2.39996323f;
		Vector3 position = center + Vector3(
			cosf(angle) * attackRadius * ratio,
			Random::Range(0.0f, 0.05f),
			sinf(angle) * attackRadius * ratio);
		breakParticleSystem->Set(
			7,
			Random::Range(0.35f, 0.5f),
			position,
			Vector3::Zero,
			Vector3::Zero,
			Vector2(0.14f, 0.14f),
			false,
			24.0f,
			Color(0.35f, 0.9f, 1.0f, 0.75f));
	}

	for (int i = 0; i < 10; ++i)
	{
		const float angle = Random::Range(0.0f, pi2);
		const float speed = Random::Range(0.5f, 1.2f);
		breakParticleSystem->Set(
			7,
			0.3f,
			center + Vector3(0.0f, 0.15f, 0.0f),
			Vector3(
				cosf(angle) * speed,
				Random::Range(0.25f, 0.65f),
				sinf(angle) * speed),
			Vector3(0.0f, -2.0f, 0.0f),
			Vector2(0.16f, 0.16f),
			false,
			24.0f,
			Color(0.45f, 0.95f, 1.0f, 1.0f));
	}
}

void Aracore::SpawnBreakCrystalParticles()
{
    if (!breakParticleSystem) return;

    const int particleCount = 35 * 2;
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

void Aracore::SpawnBreakSpiderParticles()
{
    if (!breakParticleSystem) return;

    const int particleCount = 16 * 2;
    for (int i = 0; i < particleCount; ++i)
    {
        Vector3 p = transform.position;
        p.x += Random::Range(-0.3f, 0.3f);
        p.y += Random::Range(+0.7f, 1.0f);
        p.z += Random::Range(-0.3f, 0.3f);

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
    : Actor("AracoreMachine")
{
    std::shared_ptr<VMDLModel> model =
        ResourceManager::Instance().LoadModel("Data/Model/Prop/crystal");

    // 親の体に追従
    Transform offset{};
    offset.SetPosition(0.0f, 0.0f, 0.0f);
    offset.SetAngle(-90.0f, 0.0f, 13.9f);
    offset.SetScale(1.1f, 0.5f, 0.9f);
    AddComponent<BoneFollower>(ownerAracore->model.get(),
        "Bone.004_012", offset);

    // モデルレンダラー
    AddComponent<VMDLModelComponent>(model, ModelShaderId::VMat);
}
