#include "Gameplay/Actor/AracoreQueen.h"
#include "Animation/Animator.h"

#include <algorithm>
#include <cstdlib>
#include "Physics/Collider/CharacterController.h"
#include "Resource/VMDLModel.h"
#include "Rendering/Component/VMDL.h"
#include "Rendering/Component/VMDLModelComponent.h"
#include "Physics/Core/PhysicsComponent.h"
#include "Physics/Collider/CapsuleCollider.h"
#include "Physics/Collider/SphereCollider.h"
#include "Physics/Collider/VMDLColliderComponent.h"
#include "Physics/Navigation/NavMeshAgent.h"
#include "Gameplay/Scene/CameraEffectController.h"
#include "Gameplay/Scene/TimeScaleController.h"
#include "Application/Time/GameTime.h"
#include "Animation/MultiLegFootIK.h"
#include "Gameplay/Player/Player.h"
#include "Gameplay/Actor/Spawner.h"
#include "Gameplay/Stage/Component/StageLoader.h"
#include "Gameplay/Stage/Component/Terrain.h"
#include "Audio/SoundSystem.h"
#include "UI/BossBar.h"

AracoreQueen::~AracoreQueen()
{
	if (bossBar) bossBar->Hide(this);
	StopChaseBgm();
}

AracoreQueen::AracoreQueen(Player* player_init,
    Vector3 position, Terrain* terrain_init) : Entity("Aracore Queen", "Enemy", true, 1000.0f, 1000.0f)
{
	this->player = player_init;
	this->terrain = terrain_init;
	this->spawnPosition = position;

    // 蜘蛛の部分
    {
        // モデル
        vmdl = AddComponent<VMDL>("Resources/Model/Enemy/aracore");
        vmdl->SetAutoUpdateTransform(false);
        model = vmdl->GetSharedModel();
        transform.SetPosition(position);
        transform.SetScale(1.0f);
        vmdl->UpdateTransform(transform.matrix);

        // アニメータ
        anim = vmdl->GetAnimator();
        anim->SetRootMotion("Box01");
        anim->Load("Resources/Animator/animated_spider.animator");
        anim->BindCallbacks();

        // キャラクターコントローラー
        characterController = AddComponent<CharacterController>(
            Layers::Get("Foot"), 3.0f, 0.01f);
        characterController->SetPushable(false);
        characterController->SetStepOffset(0.0f);
        characterController->SetConstrainedClimbing(true);
        characterController->SetSlopeLimitDeg(70.0f);
        characterController->SetContactOffset(0.2f);
        navMeshAgent = AddComponent<NavMeshAgent>();
        navMeshAgent->SetTurnSpeed(1.8f);

        multiLegFootIK = vmdl->GetMultiLegFootIK();
    }

    controller = AddComponent<EnemyAIFlow>();
    controller->SetGraphPath("Resources/AI/SpiderChase.json");
    if (!controller->Load(controller->GetGraphPath()))
        controller->CreateDefaultChaseGraph();
    controller->SetAgentRadius(3.0f);
    controller->SetTrackingTurnSpeed(2.0f);

    // AIパラメータの初期値
    const auto ensureFloat = [this](const std::string& name, float value)
    {
        for (const AIFlow::Parameter& parameter : controller->GetParameters())
            if (parameter.name == name) return;
        controller->SetFloat(name, value);
    };
    ensureFloat("FreedomMinDistance", 5.0f);
    ensureFloat("FreedomMaxDistance", 18.0f);
    ensureFloat("FreedomMoveSpeed", 1.4f);
    ensureFloat("FreedomWaitDuration", 1.5f);
    ensureFloat("ThreatDuration", 3.0f);
    ensureFloat("ChaseMoveSpeed", 3.2f);
    ensureFloat("ChaseDuration", 6.0f);
    ensureFloat("TurningMoveSpeed", 1.4f);
    ensureFloat("ChargePrepareDuration", 1.0f);
    ensureFloat("ChargeMoveSpeed", 6.5f);
    ensureFloat("ChargeOverDistance", 10.0f);
    ensureFloat("ChargeMaxDuration", 2.5f);
    ensureFloat("RecoveryDuration", 1.5f);
    ensureFloat("JumpChance", 0.25f);
    ensureFloat("JumpDuration", 2.4f);
    ensureFloat("JumpHeight", 10.0f);
    ensureFloat("JumpLandingRecovery", 0.9f);

    const auto stop = [this](const EnemyAIFlow::State&)
    {
        controller->StopMovement();
        StopAnimatedMovement();
    };
    const auto enterFreedomWait = [this](const EnemyAIFlow::State&)
    {
        controller->StopMovement();
        StopAnimatedMovement();
        controller->SetBool("ActionFinished", false, true);
    };
    const auto updateFreedomWait = [this](const EnemyAIFlow::State&)
    {
        controller->SetBool(
            "ActionFinished",
            controller->GetFloat("StateTime") >=
                controller->GetFloat("FreedomWaitDuration", 1.5f),
            true);
    };
    const auto enterWander = [this](const EnemyAIFlow::State&)
    {
        const float moveSpeed = controller->GetFloat("FreedomMoveSpeed", 1.4f);
        RequestAnimatedMovement(moveSpeed, "walk");
        navMeshAgent->SetMovementPaused(false);
        navMeshAgent->SetSpeed(moveSpeed);
        navMeshAgent->MoveToRandomPosition(
            controller->GetFloat("FreedomMinDistance", 5.0f),
            controller->GetFloat("FreedomMaxDistance", 18.0f));
    };
    const auto updateTurn = [this](const EnemyAIFlow::State&)
    {
		// 既存の経路をたどり続けることで、大きな進行方向の変化が歩行弧になります
        const float moveSpeed = controller->GetFloat("FreedomMoveSpeed", 1.4f);
        RequestAnimatedMovement(moveSpeed, "walk");
        navMeshAgent->SetSpeed(moveSpeed);
    };
    const auto exitTurn = [this](const EnemyAIFlow::State&)
    {
        navMeshAgent->SetMovementPaused(false);
    };
    const auto enterThreat = [this](const EnemyAIFlow::State&)
    {
		SetPlayerDetected(true);
        controller->StopMovement();
        StopAnimatedMovement();
        controller->SetBool("ActionFinished", false, true);
        controller->FaceTarget();
		Actor* target = controller->GetTarget();
		if (target != threatenedTarget)
		{
			threatenedTarget = target;
			PlayThreatPresentation();
		}
    };
    const auto updateThreat = [this](const EnemyAIFlow::State&)
    {
        controller->FaceTarget();
        controller->SetBool(
            "ActionFinished",
            controller->GetFloat("StateTime") >= controller->GetFloat("ThreatDuration", 3.0f),
            true);
    };
    const auto enterChase = [this](const EnemyAIFlow::State&)
    {
		chaseBgmEngaged = true;
		SetPlayerDetected(true);
        RequestAnimatedMovement(controller->GetFloat("ChaseMoveSpeed", 3.2f), "walk");
        controller->SetBool("ActionFinished", false, true);
    };
    const auto updateChase = [this](const EnemyAIFlow::State&)
    {
        if (Actor* target = controller->GetTarget())
        {
            const float moveSpeed = controller->GetFloat("ChaseMoveSpeed", 3.2f);
            RequestAnimatedMovement(moveSpeed, "walk");
            navMeshAgent->SetSpeed(moveSpeed);
            navMeshAgent->MoveToTarget(target);
        }
        controller->SetBool(
            "ActionFinished",
            controller->GetFloat("StateTime") >= controller->GetFloat("ChaseDuration", 6.0f),
            true);
	};
    const auto enterChargeTurn = [this](const EnemyAIFlow::State&)
    {
        RequestAnimatedMovement(controller->GetFloat("TurningMoveSpeed", 1.4f), "walk");
        controller->SetBool("ActionFinished", false, true);
    };
    const auto updateChargeTurn = [this](const EnemyAIFlow::State&)
    {
        if (Actor* target = controller->GetTarget())
        {
            const float moveSpeed = controller->GetFloat("TurningMoveSpeed", 1.4f);
            RequestAnimatedMovement(moveSpeed, "walk");
            navMeshAgent->SetSpeed(moveSpeed);
            navMeshAgent->MoveToTarget(target);
        }
        controller->SetBool(
            "ActionFinished",
            controller->GetFloat("StateTime") >=
                controller->GetFloat("ChargePrepareDuration", 1.0f),
            true);
	};
    const auto enterCharge = [this](const EnemyAIFlow::State&)
    {
        controller->StopMovement();
        controller->SetBool("ActionFinished", false, true);
        const float chargeSpeed = controller->GetFloat("ChargeMoveSpeed", 6.5f);
        RequestAnimatedMovement(chargeSpeed, "charge");
        anim->SetTrigger("Attack");
        Actor* target = controller->GetTarget();
        if (!target) return;

        Vector3 direction = target->transform.position - transform.position;
        direction.y = 0.0f;
        if (direction.LengthSquared() > eps) direction.Normalize();
        else direction = transform.forward;

        const Vector3 destination = target->transform.position +
            direction * controller->GetFloat("ChargeOverDistance", 10.0f);
        navMeshAgent->SetSpeed(chargeSpeed);
        navMeshAgent->MoveToPosition(destination);
    };
    const auto updateCharge = [this](const EnemyAIFlow::State&)
    {
        RequestAnimatedMovement(controller->GetFloat("ChargeMoveSpeed", 6.5f), "charge");
        controller->SetBool(
            "ActionFinished",
            controller->GetFloat("StateTime") >=
                controller->GetFloat("ChargeMaxDuration", 2.5f),
            true);
    };
    const auto enterRecovery = [this](const EnemyAIFlow::State&)
    {
        controller->StopMovement();
        StopAnimatedMovement();
        controller->SetBool("ActionFinished", false, true);
        const float chance = std::clamp(controller->GetFloat("JumpChance", 0.25f), 0.0f, 1.0f);
        const float roll = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
        controller->SetBool("ShouldJump", roll < chance, true);
    };
    const auto updateRecovery = [this](const EnemyAIFlow::State&)
    {
        controller->SetBool(
            "ActionFinished",
            controller->GetFloat("StateTime") >= controller->GetFloat("RecoveryDuration", 1.5f),
            true);
    };
    const auto enterJump = [&](const EnemyAIFlow::State&)
    {
        controller->StopMovement();
        StopAnimatedMovement();
        controller->SetBool("ActionFinished", false, true);
        jumpStartPosition = transform.position;
		jumpLandingPosition = this->player ? this->player->transform.position : jumpStartPosition;
		jumpLanded = false;
		landingDeformPending = false;
        characterController->SetUseGravity(false);
        anim->SetTrigger("Jump");

		Vector3 direction = jumpLandingPosition - transform.position;
        direction.y = 0.0f;
        if (direction.LengthSquared() > eps)
        {
            const float targetYaw = atan2f(direction.x, direction.z);
            transform.SetRotation(Quaternion::CreateFromYawPitchRoll(targetYaw, 0.0f, 0.0f));
        }
    };
    const auto updateJump = [&](const EnemyAIFlow::State&)
    {
        const float duration = std::max(controller->GetFloat("JumpDuration", 2.4f), 0.01f);
		const float stateTime = controller->GetFloat("StateTime");
		const float t = std::clamp(stateTime / duration, 0.0f, 1.0f);
		Vector3 position = Vector3::Lerp(jumpStartPosition, jumpLandingPosition, t);
        position.y += 4.0f * controller->GetFloat("JumpHeight", 10.0f) * t * (1.0f - t);
        characterController->SetPosition(position);
		if (!jumpLanded && t >= 1.0f)
		{
			jumpLanded = true;
			characterController->SetUseGravity(true);
			SpawnDeerFromSky();
			ShakeCameraAtLanding();
			landingDeformPending = terrain != nullptr;
		}
		const float landingRecovery =
			std::max(controller->GetFloat("JumpLandingRecovery", 0.9f), 0.0f);
		controller->SetBool("ActionFinished", jumpLanded && stateTime >= duration + landingRecovery, true);
    };
    const auto exitJump = [&](const EnemyAIFlow::State&)
    {
		characterController->SetPosition(jumpLandingPosition);
        characterController->SetUseGravity(true);
    };

    controller->AddCallbackFunc("FreedomWait", enterFreedomWait, updateFreedomWait);
    controller->AddCallbackFunc("FreedomWander", enterWander);
    controller->AddCallbackFunc("WanderTurn", {}, updateTurn, {}, exitTurn);
    controller->AddCallbackFunc("Threat", enterThreat, updateThreat, {}, stop);
	controller->AddCallbackFunc("Chase", enterChase, updateChase, {}, stop);
    controller->AddCallbackFunc("ChargeTurn", enterChargeTurn, updateChargeTurn, {}, stop);
    controller->AddCallbackFunc("Charge", enterCharge, updateCharge, {}, stop);
    controller->AddCallbackFunc("Recovery", enterRecovery, updateRecovery, {}, stop);
    controller->AddCallbackFunc("JumpCenter", enterJump, updateJump, {}, exitJump);
    controller->BindCallbacks();
}

void AracoreQueen::SpawnDeerFromSky()
{
	if (!stageLoader) return;

	constexpr float spawnHeight = 12.0f;
	for (Spawner* spawner : stageLoader->GetSpawners())
	{
		if (!spawner || spawner->GetEntityName() != "EnemySmall") continue;
		if (std::rand() % 3 != 0) continue;

		Transform spawnTransform = spawner->GetSummonTransform();
		spawnTransform.position.y += spawnHeight;
		spawnTransform.Update();
		spawner->Summon(spawnTransform);
	}
}

void AracoreQueen::ShakeCameraAtLanding()
{
	if (!player) return;

	constexpr float fullShakeDistance = 6.0f;
	constexpr float noShakeDistance = 45.0f;
	constexpr float maximumDuration = 0.55f;
	constexpr float maximumIntensity = 0.50f;

	const float distance = (player->transform.position - jumpLandingPosition).Length();
	float attenuation = 1.0f - std::clamp(
		(distance - fullShakeDistance) / (noShakeDistance - fullShakeDistance),
		0.0f,
		1.0f);
	attenuation = attenuation * attenuation * (3.0f - 2.0f * attenuation);
	if (attenuation <= 0.0f) return;

	CameraEffectController::Request(
		0.18f + (maximumDuration - 0.18f) * attenuation,
		maximumIntensity * attenuation);
}

void AracoreQueen::RequestAnimatedMovement(float speed, const char* requiredAnimation)
{
    requestedAnimationMoveSpeed = std::max(speed, 0.0f);
    requiredMovementAnimation = requiredAnimation ? requiredAnimation : "";
    movementAnimationGateActive = !requiredMovementAnimation.empty();
    if (anim) anim->SetFloat("speed", requestedAnimationMoveSpeed);
    if (navMeshAgent) navMeshAgent->SetMovementPaused(!IsMovementAnimationReady());
}

void AracoreQueen::StopAnimatedMovement()
{
    requestedAnimationMoveSpeed = 0.0f;
    requiredMovementAnimation.clear();
    movementAnimationGateActive = false;
    if (anim) anim->SetFloat("speed", 0.0f);
    if (navMeshAgent) navMeshAgent->SetMovementPaused(false);
}

bool AracoreQueen::IsMovementAnimationReady() const
{
    if (!movementAnimationGateActive || !anim) return true;
    if (anim->IsTransitioning()) return false;

    const std::string& state = anim->GetCurrentStateName();
    if (requiredMovementAnimation == "charge")
        return state == "attack" || state == "run";
    return state == requiredMovementAnimation;
}

// 威嚇アニメーションを開始する。画面演出はVMDLのタイムラインキーから再生される。
void AracoreQueen::PlayThreatPresentation()
{
    if (anim) anim->SetTrigger("Threat");
}

// 現在の索敵対象ごとに、威嚇演出が必ず一度は再生されるよう保証する。
void AracoreQueen::UpdateThreatPresentation()
{
    if (!controller) return;

	Actor* target = controller->GetTarget();
	if (!target)
	{
		threatenedTarget = nullptr;
		SetPlayerDetected(false);
		return;
	}
	// 距離検索で候補に入っただけでは発見扱いにしない。視野角と遮蔽物判定を通過してから威嚇する。
	if (!controller->GetBool("IsTargetVisible")) return;
	SetPlayerDetected(true);
	if (target == threatenedTarget) return;

    threatenedTarget = target;
    PlayThreatPresentation();
}

void AracoreQueen::OnUpdate()
{
	if (landingDeformPending)
	{
		landingDeformPending = false;
		DeformTerrainAtLanding();
	}
    Entity::OnUpdate();
	if (deathSequenceActive)
	{
		UpdateDeathSequence();
		return;
	}
    anim->SetFloat("speed", requestedAnimationMoveSpeed);
    if (movementAnimationGateActive)
        navMeshAgent->SetMovementPaused(!IsMovementAnimationReady());
}

void AracoreQueen::OnLateUpdate()
{
	if (!IsDead()) UpdateThreatPresentation();
	UpdateChaseBgm();

    // The eight EnemyAtk attachments are the foot hitboxes. Walking feet can hit,
    // and they must remain active throughout the Charge AI state even while the
    // animator is blending from walk to attack/run.
    if (VMDLModelComponent* renderer = vmdl ? vmdl->GetRenderer() : nullptr)
    {
        const bool walking = anim && !anim->IsTransitioning() &&
            anim->GetCurrentStateName() == "walk";
        const AIFlow::State* aiState = controller ? controller->GetCurrentState() : nullptr;
        const bool charging = aiState && aiState->callbackName == "Charge";
        renderer->SetAttachmentLayerEnabled(
            Layers::Get("EnemyAtk"), walking || charging);
    }
    UpdateAnimatedModelTransform();
}

void AracoreQueen::UpdateChaseBgm()
{
	if (IsDead() || !controller || !controller->GetBool("IsTargetInLostRange"))
	{
		chaseBgmEngaged = false;
		SetPlayerDetected(false);
	}

	SoundSystem& sound = SoundSystem::Instance();
	if (chaseBgmEngaged && !sound.IsPlaying(voiceIdChase))
	{
		SoundSystem::PlayOptions options;
		options.volume = 0.0f;
		options.loop = true;
		voiceIdChase = sound.PlayTrack(SoundTrack::BGM_CHASE, -1, options);
		chaseBgmVolume = 0.0f;
	}

	if (voiceIdChase == SoundSystem::InvalidVoiceId || !sound.IsPlaying(voiceIdChase))
	{
		voiceIdChase = SoundSystem::InvalidVoiceId;
		chaseBgmVolume = 0.0f;
		return;
	}

	const float deltaTime = std::max(Game::Time::unscaledDeltaTime, 0.0f);
	if (chaseBgmEngaged)
	{
		const float duration = std::max(chaseBgmFadeInSeconds, 0.001f);
		chaseBgmVolume = std::min(chaseBgmVolume + deltaTime / duration, 1.0f);
	}
	else
	{
		const float duration = std::max(chaseBgmFadeOutSeconds, 0.001f);
		chaseBgmVolume = std::max(chaseBgmVolume - deltaTime / duration, 0.0f);
	}

	if (chaseBgmVolume <= 0.0f && !chaseBgmEngaged)
	{
		StopChaseBgm();
		return;
	}

	if (!sound.SetVolume(voiceIdChase, chaseBgmVolume)) StopChaseBgm();
}

void AracoreQueen::StopChaseBgm()
{
	if (voiceIdChase != SoundSystem::InvalidVoiceId)
		SoundSystem::Instance().Stop(voiceIdChase);
	voiceIdChase = SoundSystem::InvalidVoiceId;
	chaseBgmVolume = 0.0f;
}

// ボスの着地点を中心にTerrainをへこませる
void AracoreQueen::DeformTerrainAtLanding()
{
	if (!terrain || !controller) return;
	constexpr float power = 1.5f; // 強さ
	constexpr float radius = 8.5f; // 半径
	terrain->Deform(jumpLandingPosition, Vector3::Down, power, radius);
}

void AracoreQueen::UpdateAnimatedModelTransform()
{
    if (!model || !vmdl) return;

    // アニメーターは更新時にポーズ全体を書き込みます。ここでアニメーションのルートポーズを修正し、
	// ソースモデルの -Z 方向の前面がゲームプレイの +Z 方向の前面と一致するようにします
    const Matrix facingCorrection = Matrix::CreateRotationY(DirectX::XM_PI);
    for (VMDLModel::Node& node : model->GetNodes())
    {
        if (node.parentIndex >= 0) continue;

        const Matrix animatedLocal =
            Matrix::CreateScale(node.scale) *
            Matrix::CreateFromQuaternion(node.rotation) *
            Matrix::CreateTranslation(node.position);
        (animatedLocal * facingCorrection).Decompose(node.scale, node.rotation, node.position);
        node.rotation.Normalize();
    }

    vmdl->UpdateTransform(transform.matrix);
}

void AracoreQueen::OnDrawGUI()
{
    Entity::OnDrawGUI();

    if (ImGui::Button("THREAT"))
    {
        PlayThreatPresentation();
    }

	ImGui::DragFloat("Chase BGM Fade In", &chaseBgmFadeInSeconds, 0.05f, 0.0f, 10.0f, "%.2f sec");
	ImGui::DragFloat("Chase BGM Fade Out", &chaseBgmFadeOutSeconds, 0.05f, 0.0f, 10.0f, "%.2f sec");

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
	Actor* attacker = damageData.hitColliderSelf
		? dynamic_cast<Actor*>(damageData.hitColliderSelf->GetOwner())
		: nullptr;
	if (attacker && attacker->CompareTag("Player"))
	{
		if (controller) controller->LockOn(attacker);
		threatenedTarget = attacker;
		SetPlayerDetected(true);
	}
    TimeScaleController::Request(0.15f);
    CameraEffectController::Request(0.2f, 0.1f);
}

void AracoreQueen::OnDead(const DamageData& damageData)
{
	SetPlayerDetected(false);
	StopChaseBgm();
	if (controller)
	{
		controller->StopMovement();
		controller->SetActive(false);
	}
	StopAnimatedMovement();
	if (navMeshAgent) navMeshAgent->SetMovementPaused(true);

	deathSequenceActive = true;
	deathAnimationStarted = false;
	deathThreatStarted = false;
	deathSequenceTimer = 0.0f;
	deathJumpStartPosition = transform.position;
	jumpLandingPosition = spawnPosition;
	knockBackVelocity = Vector3::Zero;
	if (characterController) characterController->SetUseGravity(false);

	Vector3 direction = jumpLandingPosition - deathJumpStartPosition;
	direction.y = 0.0f;
	if (direction.LengthSquared() > eps)
	{
		direction.Normalize();
		const float targetYaw = atan2f(direction.x, direction.z);
		transform.SetRotation(Quaternion::CreateFromYawPitchRoll(targetYaw, 0.0f, 0.0f));
	}
	if (anim)
	{
		anim->SetBool("Dead", false);
		anim->SetFloat("speed", 0.0f);
		anim->SetTrigger("Jump");
	}
	if (player)
		player->RequestBossDefeatCamera(
			this, deathJumpDuration + deathThreatDuration + 2.4f);
    printf("AracoreQueen Dead!\n");
    //Destroy(5);
}

void AracoreQueen::UpdateDeathSequence()
{
	deathSequenceTimer += std::max(Game::Time::unscaledDeltaTime, 0.0f);
	const float duration = std::max(deathJumpDuration, 0.01f);
	const float t = std::clamp(deathSequenceTimer / duration, 0.0f, 1.0f);
	Vector3 position = Vector3::Lerp(deathJumpStartPosition, jumpLandingPosition, t);
	position.y += 4.0f * deathJumpHeight * t * (1.0f - t);
	if (characterController) characterController->SetPosition(position);
	else transform.SetPosition(position);

	if (t < 1.0f) return;
	if (!deathThreatStarted)
	{
		deathThreatStarted = true;
		if (characterController)
		{
			characterController->SetPosition(jumpLandingPosition);
			characterController->SetUseGravity(true);
		}
		else transform.SetPosition(jumpLandingPosition);

		Vector3 faceDirection = player
			? player->transform.position - jumpLandingPosition : transform.forward;
		faceDirection.y = 0.0f;
		if (faceDirection.LengthSquared() > eps)
		{
			faceDirection.Normalize();
			transform.SetRotation(Quaternion::CreateFromYawPitchRoll(
				atan2f(faceDirection.x, faceDirection.z), 0.0f, 0.0f));
		}
		PlayThreatPresentation();
		ShakeCameraAtLanding();
	}

	if (deathAnimationStarted ||
		deathSequenceTimer < deathJumpDuration + deathThreatDuration) return;
	deathAnimationStarted = true;
	deathSequenceActive = false;
	if (anim) anim->SetBool("Dead", true);
}

void AracoreQueen::SetBossBar(BossBar* value)
{
	if (bossBar && bossBar != value) bossBar->Hide(this);
	bossBar = value;
	if (!bossBar) return;
	if (playerDetected && !IsDead()) bossBar->Show(this);
	else bossBar->Hide(this);
}

void AracoreQueen::SetPlayerDetected(bool detected)
{
	playerDetected = detected && !IsDead();
	if (!bossBar) return;
	if (playerDetected) bossBar->Show(this);
	else bossBar->Hide(this);
}
