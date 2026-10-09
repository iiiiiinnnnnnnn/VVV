#include "Gameplay/Actor/Aracore.h"
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
#include "Gameplay/Camera/ThirdPersonCameraController.h"
#include "Application/Time/GameTime.h"
#include "Animation/MultiLegFootIK.h"
#include "Gameplay/Player/Player.h"
#include "Gameplay/Actor/ActorManager.h"
#include "Gameplay/Actor/Spawner.h"
#include "Gameplay/Stage/Component/StageLoader.h"
#include "Gameplay/Stage/Component/Terrain.h"
#include "Audio/SoundSystem.h"
#include "UI/BossBar.h"
#include "Gameplay/Actor/Crystal.h"
#include "Resource/ResourceManager.h"

Aracore::~Aracore()
{
	if (bossBar) bossBar->Hide(this);
	StopChaseBgm();
}

Aracore::Aracore(Player* player_init,
	const Transform& transform, Terrain* terrain_init, const std::string& modelPath)
	: Entity("Aracore", "Enemy", true, transform, 3000.0f, 3000.0f)
{
	this->player = player_init;
	this->terrain = terrain_init;
	this->spawnPosition = transform.position;

	// 蜘蛛の部分
	{
		// モデル
		vmdl = AddComponent<VMDL>(modelPath);
		vmdl->SetAutoUpdateTransform(false);
		vmdl->SetModelYawOffset(DirectX::XM_PI);
		model = vmdl->GetSharedModel();
		this->transform = transform;
		vmdl->UpdateTransform(transform.matrix);

		// アニメータ
		anim = vmdl->GetAnimator();
		anim->SetRootMotion("Box01");
		anim->Load("Resources/Animator/Aracore.animator");
		anim->BindCallbacks();

		// キャラクターコントローラー
		characterController = AddComponent<CharacterController>(
			Layers::Get("Enemy"), 1.84f, 0.01f);

		// 移動の衝突対象は元のFootレイヤーと同じ範囲に保つ
		characterController->SetLayerIgnored(Layers::Get("Default"), true);
		characterController->SetLayerIgnored(Layers::Get("Player"), true);
		characterController->SetLayerIgnored(Layers::Get("Enemy"), true);
		characterController->SetPushable(false);
		characterController->SetStepOffset(0.0f);
		characterController->SetConstrainedClimbing(true);
		characterController->SetSlopeLimitDeg(70.0f);
		characterController->SetContactOffset(0.2f);
		navMeshAgent = AddComponent<NavMeshAgent>();
		navMeshAgent->SetTurnSpeed(1.8f);

		multiLegFootIK = vmdl->GetMultiLegFootIK();
	}

	ConfigureAI();
	RegisterAICallbacks();

	// 破片のモデルは、形態変化が起こる前に準備する。
	phaseFragmentModel = ResourceManager::Instance().LoadModel("Resources/Model/CrystalPart.vmdl", true);
	Vector3 fragmentCenter;
	if (phaseFragmentModel && GetCrystalModelBounds(*phaseFragmentModel, fragmentCenter, phaseFragmentSize))
		phaseFragmentModel->GetVmdlExtensionData().rootOffset -= fragmentCenter;
}

void Aracore::ConfigureAI()
{
	controller = AddComponent<EnemyAIFlow>();
	controller->SetGraphPath("Resources/AI/Aracore.json");
	if (!controller->Load(controller->GetGraphPath()))
		controller->CreateDefaultChaseGraph();
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
	ensureFloat("JumpTakeoffDelay", 0.65f);
	ensureFloat("JumpDuration", 2.4f);
	ensureFloat("JumpHeight", 10.0f);
	ensureFloat("JumpLandingRecovery", 0.9f);
	ensureFloat("EnragedIdleScale", 0.4f);
	ensureFloat("EnragedJumpChance", 0.7f);
	ensureFloat("EnragedJumpCount", 2.0f);
	ensureFloat("EnragedLowHpJumpCount", 3.0f);
	ensureFloat("PhaseThreatDuration", 3.2f);
	ensureFloat("PhaseColorChangeDuration", 3.0f);
	ensureFloat("DeathDissolveAmount", 0.35f);

}

void Aracore::RegisterAICallbacks()
{
	// 共通ストップ
	const auto stop = [this](const EnemyAIFlow::State&)
	{
		controller->StopMovement();
		StopAnimatedMovement();
	};

	// 自由行動
	const auto enterFreedomWait = [this](const EnemyAIFlow::State&)
	{
		controller->StopMovement();
		StopAnimatedMovement();
		controller->SetBool("ActionFinished", false, true);
	};

	// 自由行動待機時間
	const auto updateFreedomWait = [this](const EnemyAIFlow::State&)
	{
		const float stateTime = controller->GetFloat("StateTime");
		const float duration = controller->GetFloat("FreedomWaitDuration", 1.5f);
		const bool actionFinished = stateTime >= duration;
		controller->SetBool("ActionFinished", actionFinished, true);
	};

	// 自由行動
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

	// 旋回中(歩かない)
	const auto updateTurn = [this](const EnemyAIFlow::State&)
	{
		// 既存の経路をたどり続けることで、大きな進行方向の変化が歩行弧になります
		const float moveSpeed = controller->GetFloat("FreedomMoveSpeed", 1.4f);
		RequestAnimatedMovement(moveSpeed, "walk");
		navMeshAgent->SetSpeed(moveSpeed);
	};

	// 旋回終了
	const auto exitTurn = [this](const EnemyAIFlow::State&)
	{
		navMeshAgent->SetMovementPaused(false);
	};

	// 威嚇演出
	const auto enterThreat = [this](const EnemyAIFlow::State&)
	{
		SetPlayerDetected(true);
		controller->StopMovement();
		StopAnimatedMovement();
		controller->SetBool("ActionFinished", false, true);
		//controller->FaceTarget();
		Actor* target = controller->GetTarget();
		if (target != threatenedTarget)
		{
			threatenedTarget = target;
			PlayThreatPresentation();
		}
	};

	// 威嚇演出更新
	const auto updateThreat = [this](const EnemyAIFlow::State&)
	{
		const float stateTime = controller->GetFloat("StateTime");
		const float duration = controller->GetFloat("ThreatDuration", 3.0f);
		const bool actionFinished = stateTime >= duration;
		controller->SetBool("ActionFinished", actionFinished, true);
	};

	// 追跡開始
	const auto enterChase = [this](const EnemyAIFlow::State&)
	{
		chaseBgmEngaged = true;
		SetPlayerDetected(true);
		RequestAnimatedMovement(controller->GetFloat("ChaseMoveSpeed", 3.2f), "walk");
		controller->SetBool("ActionFinished", false, true);
	};

	// 追跡更新
	const auto updateChase = [this](const EnemyAIFlow::State&)
	{
		if (Actor* target = controller->GetTarget())
		{
			const float moveSpeed = controller->GetFloat("ChaseMoveSpeed", 3.2f);
			RequestAnimatedMovement(moveSpeed, "walk");
			navMeshAgent->SetSpeed(moveSpeed);
			navMeshAgent->MoveToTarget(target);
		}
		const float stateTime = controller->GetFloat("StateTime");
		const float duration = controller->GetFloat("ChaseDuration", 6.0f);
		const bool actionFinished = stateTime >= duration;
		controller->SetBool("ActionFinished", actionFinished, true);
	};

	// 突進旋回開始
	const auto enterChargeTurn = [this](const EnemyAIFlow::State&)
	{
		RequestAnimatedMovement(controller->GetFloat("TurningMoveSpeed", 1.4f), "walk");
		controller->SetBool("ActionFinished", false, true);
	};

	// 突進旋回更新
	const auto updateChargeTurn = [this](const EnemyAIFlow::State&)
	{
		if (Actor* target = controller->GetTarget())
		{
			const float moveSpeed = controller->GetFloat("TurningMoveSpeed", 1.4f);
			RequestAnimatedMovement(moveSpeed, "walk");
			navMeshAgent->SetSpeed(moveSpeed);
			navMeshAgent->MoveToTarget(target);
		}
		const float stateTime = controller->GetFloat("StateTime");
		const float duration = controller->GetFloat("ChargePrepareDuration", 1.0f);
		const bool actionFinished = stateTime >= duration;
		controller->SetBool("ActionFinished", actionFinished, true);
	};

	// 突進開始
	const auto enterCharge = [this](const EnemyAIFlow::State&)
	{
		controller->StopMovement();
		controller->SetBool("ActionFinished", false, true);
		const float chargeSpeed = controller->GetFloat("ChargeMoveSpeed", 6.5f);
		RequestAnimatedMovement(chargeSpeed, "charge");
		anim->SetTrigger("Attack");
		Actor* target = controller->GetTarget();
		if (!target) return;

		Vector3 direction = target->transform.position - this->transform.position;
		direction.y = 0.0f;
		if (direction.LengthSquared() > eps) direction.Normalize();
		else direction = this->transform.forward;

		const Vector3 destination = target->transform.position +
			direction * controller->GetFloat("ChargeOverDistance", 10.0f);
		navMeshAgent->SetSpeed(chargeSpeed);
		navMeshAgent->MoveToPosition(destination);
	};

	// 突進更新
	const auto updateCharge = [this](const EnemyAIFlow::State&)
	{
		RequestAnimatedMovement(controller->GetFloat("ChargeMoveSpeed", 6.5f), "charge");
		const float stateTime = controller->GetFloat("StateTime");
		const float duration = controller->GetFloat("ChargeMaxDuration", 2.5f);
		const bool actionFinished = stateTime >= duration;
		controller->SetBool("ActionFinished", actionFinished, true);
	};

	// 回復開始
	const auto enterRecovery = [this](const EnemyAIFlow::State&)
	{
		remainingJumps = 0;
		controller->StopMovement();
		StopAnimatedMovement();
		controller->SetBool("ActionFinished", false, true);
		const float chance = std::clamp(controller->GetFloat("JumpChance", 0.25f), 0.0f, 1.0f);
		const float roll = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
		controller->SetBool("ShouldJump", roll < chance, true);
	};

	// 回復更新
	const auto updateRecovery = [this](const EnemyAIFlow::State&)
	{
		const float stateTime = controller->GetFloat("StateTime");
		const float duration = controller->GetFloat("RecoveryDuration", 1.5f);
		const bool actionFinished = stateTime >= duration;
		controller->SetBool("ActionFinished", actionFinished, true);
	};

	// ジャンプ開始
	const auto enterJump = [&](const EnemyAIFlow::State&)
	{
		if (remainingJumps <= 0) remainingJumps = GetJumpSequenceCount();
		--remainingJumps;
		controller->SetBool("ContinueJump", false, true);
		controller->StopMovement();
		StopAnimatedMovement();
		controller->SetBool("ActionFinished", false, true);
		jumpStartPosition = this->transform.position;
		jumpLandingPosition = this->player ? this->player->transform.position : jumpStartPosition;
		jumpLanded = false;
		jumpAnimationWaitingForLanding = true;
		landingDeformPending = false;
		characterController->SetUseGravity(false);
		jumpAnimationPending = true;
		if (anim->GetLayerCount() > 0 && anim->GetCurrentStateName() == "Jump" && anim->GetNextStateName().empty())
		{
			auto& layer = anim->GetLayer(0);
			layer.currentTime = 0.0f;
			layer.states[layer.currentStateIndex].speed = 1.0f;
			jumpAnimationPending = false;
		}
		else anim->SetTrigger("Jump");

		Vector3 direction = jumpLandingPosition - this->transform.position;
		direction.y = 0.0f;
		if (direction.LengthSquared() > eps)
		{
			const float targetYaw = atan2f(direction.x, direction.z);
			this->transform.SetRotation(Quaternion::CreateFromYawPitchRoll(targetYaw, 0.0f, 0.0f));
		}
	};

	// ジャンプ更新
	const auto updateJump = [&](const EnemyAIFlow::State&)
	{
		const float takeoffDelay =
			std::max(controller->GetFloat("JumpTakeoffDelay", 0.65f), 0.0f);
		const float duration = std::max(controller->GetFloat("JumpDuration", 2.4f), 0.01f);
		const float stateTime = controller->GetFloat("StateTime");
		const float flightTime = stateTime - takeoffDelay;
		const float flightProgress = flightTime / duration;
		const float t = std::clamp(flightProgress, 0.0f, 1.0f);

		const float jumpHeight = controller->GetFloat("JumpHeight", 10.0f);
		const float arcShape = 4.0f * t * (1.0f - t);
		const float heightOffset = jumpHeight * arcShape;
		Vector3 position = Vector3::Lerp(jumpStartPosition, jumpLandingPosition, t);
		position.y += heightOffset;
		characterController->SetPosition(position);
		if (!jumpLanded && t >= 1.0f)
		{
			jumpLanded = true;
			jumpAnimationWaitingForLanding = false;
			characterController->SetUseGravity(true);
			SpawnDeerFromSky();
			ShakeCameraAtLanding();
			landingDeformPending = terrain != nullptr;
		}
		const float landingRecovery =
			std::max(controller->GetFloat("JumpLandingRecovery", 0.9f), 0.0f);
		const float finishedTime = takeoffDelay + duration + landingRecovery;
		const bool actionFinished = jumpLanded && stateTime >= finishedTime;
		const bool continueJump = remainingJumps > 0 && !phaseChangeQueued;
		controller->SetBool("ContinueJump", continueJump, true);
		controller->SetBool("ActionFinished", actionFinished, true);
	};

	// ジャンプ終了
	const auto exitJump = [&](const EnemyAIFlow::State&)
	{
		jumpAnimationWaitingForLanding = false;
		characterController->SetPosition(jumpLandingPosition);
		characterController->SetUseGravity(true);
	};

	// 形態変化の演出中は、Threatの開始と演出時間の終了を待つ。
	const auto enterPhaseThreat = [this](const EnemyAIFlow::State&)
	{
		BeginPhaseChange();
	};
	const auto updatePhaseThreat = [this](const EnemyAIFlow::State&)
	{
		const float duration = controller->GetFloat("PhaseThreatDuration", 3.2f);
		const bool presentationStarted = !phasePresentationPending;
		const bool elapsedEnough = phaseThreatElapsed >= duration;
		controller->SetBool("ActionFinished", presentationStarted && elapsedEnough, true);
	};
	const auto exitPhaseThreat = [this, stop](const EnemyAIFlow::State& state)
	{
		phaseThreatActive = false;
		stop(state);
	};

	controller->AddCallbackFunc("FreedomWait", enterFreedomWait, updateFreedomWait);
	controller->AddCallbackFunc("FreedomWander", enterWander);
	controller->AddCallbackFunc("WanderTurn", {}, updateTurn, {}, exitTurn);
	controller->AddCallbackFunc("Threat", enterThreat, updateThreat, {}, stop);
	controller->AddCallbackFunc("PhaseThreat", enterPhaseThreat, updatePhaseThreat, {}, exitPhaseThreat);
	controller->AddCallbackFunc("Chase", enterChase, updateChase, {}, stop);
	controller->AddCallbackFunc("ChargeTurn", enterChargeTurn, updateChargeTurn, {}, stop);
	controller->AddCallbackFunc("Charge", enterCharge, updateCharge, {}, stop);
	controller->AddCallbackFunc("Recovery", enterRecovery, updateRecovery, {}, stop);
	controller->AddCallbackFunc("JumpCenter", enterJump, updateJump, {}, exitJump);
	controller->BindCallbacks();
}

void Aracore::SpawnDeerFromSky()
{
	if (!stageLoader) return;

	ActorManager* actorManager = ActorManager::GetActive();
	if (!actorManager) return;

	// 倒された召喚鹿を上限の対象から外す(erase_if覚える)
	auto deers = actorManager->GetActorsByName("BossFieldDeer");

	constexpr float spawnHeight = 20.0f;
	constexpr size_t maxBossSummonedDeer = 4; // 召喚最大数
	for (Spawner* spawner : stageLoader->GetSpawners("Deer"))
	{
		if (deers.size() >= maxBossSummonedDeer) break;
		if (std::rand() % 3 != 0) continue;

		Transform spawnTransform = spawner->GetSummonTransform();
		spawnTransform.position.y += spawnHeight;
		spawnTransform.Update();
		Actor* summonedDeer = spawner->Summon(spawnTransform);
	}
}

void Aracore::ShakeCameraAtLanding()
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

void Aracore::RequestAnimatedMovement(float speed, const char* requiredAnimation)
{
	requestedAnimationMoveSpeed = std::max(speed, 0.0f);
	requiredMovementAnimation = requiredAnimation ? requiredAnimation : "";
	movementAnimationGateActive = !requiredMovementAnimation.empty();
	if (anim) anim->SetFloat("speed", requestedAnimationMoveSpeed);
	if (navMeshAgent) navMeshAgent->SetMovementPaused(!IsMovementAnimationReady());
}

void Aracore::StopAnimatedMovement()
{
	requestedAnimationMoveSpeed = 0.0f;
	requiredMovementAnimation.clear();
	movementAnimationGateActive = false;
	if (anim) anim->SetFloat("speed", 0.0f);
	if (navMeshAgent) navMeshAgent->SetMovementPaused(false);
}

bool Aracore::IsMovementAnimationReady() const
{
	if (!movementAnimationGateActive || !anim) return true;
	if (anim->IsTransitioning()) return false;

	const std::string& state = anim->GetCurrentStateName();
	if (requiredMovementAnimation == "charge")
		return state == "attack" || state == "run";
	return state == requiredMovementAnimation;
}

// HP半分で一度だけ演出と攻撃パターンを切り替える。
void Aracore::BeginPhaseChange()
{
	enraged = true;
	jumpAnimationPending = false;
	phaseChangeQueued = false;
	phaseThreatActive = true;
	phasePresentationPending = true;
	phaseThreatElapsed = 0.0f;
	phaseColorElapsed = 0.0f;
	remainingJumps = 0;
	damageAnimationPending = false;
	controller->SetBool("PhaseChangeRequested", false, true);
	controller->SetBool("ActionFinished", false, true);
	controller->StopMovement();
	StopAnimatedMovement();
	ApplyEnragedAIParameters();

	if (anim->GetLayerCount() > 0 && anim->GetCurrentStateName() == "Threat")
		anim->GetLayer(0).currentTime = 0.0f;
	PlayThreatPresentation();
}

void Aracore::ApplyEnragedAIParameters()
{
	const float configuredIdleScale = controller->GetFloat("EnragedIdleScale", 0.4f);
	const float idleScale = std::clamp(configuredIdleScale, 0.1f, 1.0f);
	const char* waitParameters[] = {
		"FreedomWaitDuration",
		"ChaseDuration",
		"ChargePrepareDuration",
		"RecoveryDuration",
		"JumpLandingRecovery",
	};

	for (const char* name : waitParameters)
	{
		const float normalDuration = controller->GetFloat(name);
		const float enragedDuration = normalDuration * idleScale;
		controller->SetFloat(name, enragedDuration);
	}

	const float jumpChance = controller->GetFloat("EnragedJumpChance", 0.7f);
	controller->SetFloat("JumpChance", jumpChance);
	controller->SetTrackingTurnSpeed(4.0f);
}

int Aracore::GetJumpSequenceCount() const
{
	if (!enraged) return 1;

	const float lowHealthThreshold = GetMaxLife() * 0.25f;
	const bool hasLowHealth = GetLife() <= lowHealthThreshold;
	const char* parameter = hasLowHealth
		? "EnragedLowHpJumpCount"
		: "EnragedJumpCount";

	const float configuredCount = controller->GetFloat(parameter, 2.0f);
	const int jumpCount = static_cast<int>(configuredCount);
	return std::clamp(jumpCount, 1, 5);
}

void Aracore::SpawnPhaseFragments()
{
	auto* manager = ActorManager::GetActive();
	if (!manager || !phaseFragmentModel) return;
	constexpr int count = 6;
	constexpr float fragmentWorldSize = 2.5f;
	const float modelSize = std::max({
		phaseFragmentSize.x,
		phaseFragmentSize.y,
		phaseFragmentSize.z,
		0.001f,
	});
	const float scale = fragmentWorldSize / modelSize;
	for (int i = 0; i < count; ++i)
	{
		const float angle = DirectX::XM_2PI * i / count;
		const Vector3 direction(sinf(angle), 0.0f, cosf(angle));
		Transform placement;
		const Vector3 burstCenter = transform.position + Vector3(0.0f, 3.0f, 0.0f);
		const Vector3 radialOffset = direction * 2.0f;
		placement.position = burstCenter + radialOffset;
		placement.scale = Vector3(scale);
		placement.rotation = Quaternion::CreateFromYawPitchRoll(angle, 0.4f * i, 0.3f * i);
		placement.Update();
		auto fragment = std::make_shared<CrystalPart>(placement, phaseFragmentModel->CloneRuntimeInstance());
		manager->Register(fragment);
		const Vector3 outwardVelocity = direction * 7.0f;
		const Vector3 upwardVelocity(0.0f, 7.0f, 0.0f);
		const Vector3 angularVelocity(0.8f, 1.2f, 0.6f);
		fragment->StartMoving(outwardVelocity + upwardVelocity, angularVelocity, this);
	}
	vmdl->GetRenderer()->PlaySoundSource("DAMAGED");
	CameraEffectController::Request(0.5f, 0.15f);
}

void Aracore::UpdateHealthPresentation()
{
	const float deltaTime = std::max(Game::Time::deltaTime, 0.0f);

	// 空中では形態変化へ移らず、着地を待つ。
	const bool canChangePhase = !IsDead() && !jumpAnimationWaitingForLanding;
	if (phaseChangeQueued && canChangePhase)
		controller->SetBool("PhaseChangeRequested", true, true);

	// Threatが始まった時点から、色変化と演出時間を進める。
	if (enraged && !phasePresentationPending)
	{
		phaseColorElapsed += deltaTime;
		if (phaseThreatActive) phaseThreatElapsed += deltaTime;
	}

	UpdateHealthDissolve(deltaTime);
	UpdatePhaseColor();
}

void Aracore::UpdateHealthDissolve(float deltaTime)
{
	float healthRatio = 0.0f;
	if (GetMaxLife() > 0.0f)
	{
		const float remainingHealth = GetLife() / GetMaxLife();
		healthRatio = std::clamp(remainingHealth, 0.0f, 1.0f);
	}

	const float lostHealthRatio = 1.0f - healthRatio;
	const float deathDissolveAmount = controller->GetFloat("DeathDissolveAmount", 0.35f);
	const float targetDissolveAmount = deathDissolveAmount * lostHealthRatio;

	// フレーム時間に依存しない追従。死亡演出が終わったら目標値で固定する。
	constexpr float dissolveFollowSpeed = 4.0f;
	const float remainingDifference = expf(-dissolveFollowSpeed * deltaTime);
	const float followAmount = 1.0f - remainingDifference;
	healthDissolveAmount = std::lerp(healthDissolveAmount, targetDissolveAmount, followAmount);

	const bool deathAnimationReady = IsDead() && deathAnimationStarted;
	const float difference = std::abs(healthDissolveAmount - targetDissolveAmount);
	if (deathAnimationReady || difference < 0.0001f)
		healthDissolveAmount = targetDissolveAmount;

	auto& params = vmdl->GetRenderer()->GetRenderParams();
	params.dissolveAmount = std::clamp(healthDissolveAmount, 0.0f, 1.0f);
}

void Aracore::UpdatePhaseColor()
{
	float progress = 0.0f;
	if (enraged && !phasePresentationPending)
	{
		const float configuredDuration = controller->GetFloat("PhaseColorChangeDuration", 3.0f);
		const float duration = std::max(configuredDuration, 0.01f);
		const float elapsedRatio = phaseColorElapsed / duration;
		progress = std::clamp(elapsedRatio, 0.0f, 1.0f);
	}

	// 始めと終わりが急にならないように補間し、元の模様を残したまま赤くする。
	const float progressSquared = progress * progress;
	const float smoothProgress = progressSquared * (3.0f - 2.0f * progress);
	const Color normalTint(1.0f, 1.0f, 1.0f, 1.0f);
	const Color enragedTint(1.0f, 0.28f, 0.25f, 1.0f);

	auto& params = vmdl->GetRenderer()->GetRenderParams();
	params.baseColorTint = Color::Lerp(normalTint, enragedTint, smoothProgress);
}

void Aracore::PlayThreatPresentation()
{
	threatAnimationPending = true;
}

void Aracore::UpdateReactionAnimation()
{
	if (!anim || IsDead()) return;
	const auto& current = anim->GetCurrentStateName();
	const auto& next = anim->GetNextStateName();
	if (damageAnimationPending)
	{
		if (current == "damage" || next == "damage") damageAnimationPending = false;
		else
		{
			anim->SetTrigger("Damage");
			return;
		}
	}
	if (jumpAnimationPending && !phaseThreatActive)
	{
		if (next == "Jump" || (current == "Jump" && next.empty())) jumpAnimationPending = false;
		else
		{
			anim->SetTrigger("Jump");
			return;
		}
	}
	if (!threatAnimationPending) return;
	if (current == "Threat" || next == "Threat")
	{
		threatAnimationPending = false;
		if (phasePresentationPending)
		{
			phasePresentationPending = false;
			phaseThreatElapsed = 0.0f;
			phaseColorElapsed = 0.0f;
			SpawnPhaseFragments();
		}
		return;
	}
	if (current == "damage" || next == "damage") return;
	anim->SetTrigger("Threat");
}

// 現在の索敵対象ごとに、威嚇演出が必ず一度は再生されるよう保証する。
void Aracore::UpdateThreatPresentation()
{
	if (!controller || phaseThreatActive) return;

	Actor* target = controller->GetTarget();
	if (!target)
	{
		threatenedTarget = nullptr;
		threatAnimationPending = false;
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

void Aracore::OnUpdate()
{
	UpdateJumpAnimationLandingHold();
	if (landingDeformPending)
	{
		landingDeformPending = false;
		DeformTerrainAtLanding();
	}
	Entity::OnUpdate();
	UpdateHealthPresentation();
	if (deathSequenceActive)
	{
		UpdateDeathSequence();
		return;
	}
	anim->SetFloat("speed", requestedAnimationMoveSpeed);
	if (movementAnimationGateActive)
		navMeshAgent->SetMovementPaused(!IsMovementAnimationReady());
}

void Aracore::UpdateJumpAnimationLandingHold()
{
	if (!anim || anim->GetLayerCount() == 0) return;

	constexpr float landingAnimationTime = 1.082f;
	auto& layer = anim->GetLayer(0);
	const auto updateState = [&](int stateIndex, float& time)
	{
		if (stateIndex < 0 || stateIndex >= static_cast<int>(layer.states.size())) return;
		auto& state = layer.states[stateIndex];
		if (state.name != "Jump") return;

		if (!jumpAnimationWaitingForLanding)
		{
			if (state.speed == 0.0f) state.speed = 1.0f;
			return;
		}

		const float nextTime = time + Game::Time::deltaTime * state.speed;
		if (time < landingAnimationTime && nextTime < landingAnimationTime) return;
		time = landingAnimationTime;
		state.speed = 0.0f;
	};

	updateState(layer.currentStateIndex, layer.currentTime);
	updateState(layer.nextStateIndex, layer.nextTime);
}

void Aracore::OnLateUpdate()
{
	if (!IsDead()) UpdateThreatPresentation();
	UpdateReactionAnimation();
	UpdateChaseBgm();

	UpdateAnimatedModelTransform();
}

// 追尾中に鳴らすBGMの処理(aracoreで管理するのあまりよくないのでScene管理にしようか検討中、
// ゲームのことだけ考えるなら別にこれでいいが…)
void Aracore::UpdateChaseBgm()
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
void Aracore::StopChaseBgm()
{
	if (voiceIdChase != SoundSystem::InvalidVoiceId)
		SoundSystem::Instance().Stop(voiceIdChase);
	voiceIdChase = SoundSystem::InvalidVoiceId;
	chaseBgmVolume = 0.0f;
}

// ボスの着地点を中心にTerrainをへこませる
void Aracore::DeformTerrainAtLanding()
{
	if (!terrain || !controller) return;
	constexpr float power = 1.5f; // 強さ
	constexpr float radius = 8.5f; // 半径
	terrain->Deform(jumpLandingPosition, Vector3::Down, power, radius);
}

void Aracore::UpdateAnimatedModelTransform()
{
	if (!model || !vmdl) return;

	vmdl->UpdateTransform(transform.matrix);
}

void Aracore::OnDrawGUI()
{
	Entity::OnDrawGUI();

	if (ImGui::Button((const char*)u8"威嚇"))
	{
		PlayThreatPresentation();
	}

	if (ImGui::Button((const char*)u8"20ダメージ"))
	{
		TakeDamage(DamageData{.damage = 20.0f});
	}

	if (ImGui::Button("Kill"))
	{
		TakeDamage(DamageData{.damage = 10000.0f});
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

void Aracore::TakeDamage(const DamageData& damageData)
{
	DamageData realDMG = damageData;
	const std::string& state = anim->GetCurrentStateName();
	if (state == "Threat" || 
		threatAnimationPending)
	{
		realDMG.damage *= 0.3f;
	}
	Entity::TakeDamage(realDMG);
}

void Aracore::OnCollisionEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal)
{
	if (IsDead()) return;
	PushPlayer(self, other);

	Actor* otherActor = dynamic_cast<Actor*>(other->GetOwner());
	if (!otherActor) return;

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
}

void Aracore::OnCollisionStay(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal)
{
	if (IsDead()) return;
	PushPlayer(self, other);
}

bool Aracore::PushPlayer(PhysicsComponent* self, PhysicsComponent* other)
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

void Aracore::OnTriggerEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal)
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
		else if (otherActor->CompareTag("Crystal"))
		{
			static_cast<Entity*>(otherActor)->TakeDamage({.damage = 9999.0f});
		}
		else if (otherActor->CompareTag("Enemy"))
		{
			if (otherActor->GetName() == "Deer")
			{
				static_cast<Entity*>(otherActor)->TakeDamage({.damage = 9999.0f});
			}
		}
		return;
	}
}

void Aracore::OnDamaged(const DamageData& damageData)
{
	if (!enraged && !IsDead() && GetLife() <= GetMaxLife() * 0.5f)
		phaseChangeQueued = true;
	Actor* attacker = damageData.hitColliderSelf
		? dynamic_cast<Actor*>(damageData.hitColliderSelf->GetOwner())
		: nullptr;
	float timeStopTime = 0.15f;
	if (attacker && attacker->CompareTag("Player"))
	{
		const bool newlyDetected =
			!playerDetected || attacker != threatenedTarget;
		if (controller) controller->LockOn(attacker);
		threatenedTarget = attacker;
		SetPlayerDetected(true);
		if (newlyDetected)
		{
			PlayThreatPresentation();
		}
		if (damageData.specialAttack && !phaseThreatActive)
		{
			damageAnimationPending = true;
			timeStopTime *= 2.0f;
		}
	}
	TimeScaleController::Request(timeStopTime);
	CameraEffectController::Request(0.2f, 0.1f);

	// ダメージサウンド
	vmdl->GetRenderer()->PlaySoundSource("DAMAGED");
}

void Aracore::OnDead(const DamageData& damageData)
{
	jumpAnimationPending = false;
	phaseChangeQueued = false;
	phasePresentationPending = false;
	phaseThreatActive = false;
	remainingJumps = 0;
	controller->SetBool("PhaseChangeRequested", false, true);
	damageAnimationPending = false;
	threatAnimationPending = false;
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
		player->GetCameraController()->RequestBossDefeatFocus(
			this, deathJumpDuration + 2.4f);
	printf("Aracore Dead!\n");
	//Destroy(5);
}

void Aracore::UpdateDeathSequence()
{
	deathSequenceTimer += std::max(Game::Time::unscaledDeltaTime, 0.0f);
	const float duration = std::max(deathJumpDuration, 0.01f);
	const float flightProgress = deathSequenceTimer / duration;
	const float t = std::clamp(flightProgress, 0.0f, 1.0f);
	const float arcShape = 4.0f * t * (1.0f - t);
	const float heightOffset = deathJumpHeight * arcShape;

	Vector3 position = Vector3::Lerp(deathJumpStartPosition, jumpLandingPosition, t);
	position.y += heightOffset;
	if (characterController) characterController->SetPosition(position);
	else transform.SetPosition(position);

	if (t < 1.0f) return;
	if (deathAnimationStarted) return;
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
	ShakeCameraAtLanding();
	deathAnimationStarted = true;
	deathSequenceActive = false;
	if (anim) anim->SetBool("Dead", true);
}

void Aracore::SetBossBar(BossBar* value)
{
	if (bossBar && bossBar != value) bossBar->Hide(this);
	bossBar = value;
	if (!bossBar) return;
	if (playerDetected && !IsDead()) bossBar->Show(this);
	else bossBar->Hide(this);
}

void Aracore::SetPlayerDetected(bool detected)
{
	playerDetected = detected && !IsDead();
	if (!bossBar) return;
	if (playerDetected) bossBar->Show(this);
	else bossBar->Hide(this);
}
