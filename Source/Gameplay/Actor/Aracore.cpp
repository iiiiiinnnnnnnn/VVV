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
#include "Deer.h"

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
		anim = AddComponent<Animator>(vmdl->GetSharedModel());
		vmdl->BuildFootIK(anim);
		// 旋回はNavMesh側で制御し、威嚇・死亡時の胴体の傾きはモデルへ残す。
		anim->SetRootMotion("Box01", false);
		anim->Load("Aracore.animator");
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

		// nav
		navMeshAgent = AddComponent<NavMeshAgent>();
		navMeshAgent->SetTurnSpeed(1.8f);

		multiLegFootIK = vmdl->GetMultiLegFootIK();
	}

	ConfigureAI();
	RegisterAIActions();

	// 破片のモデルは、形態変化が起こる前に準備する。
	phaseFragmentModel = ResourceManager::Instance().LoadModel("CrystalPart", true);
	Vector3 fragmentCenter;
	if (phaseFragmentModel && GetCrystalModelBounds(*phaseFragmentModel, fragmentCenter, phaseFragmentSize))
		phaseFragmentModel->GetVmdlExtensionData().rootOffset -= fragmentCenter;
}

void Aracore::ConfigureAI()
{
	// Enemy AI
	controller = AddComponent<EnemyAI>("Aracore.lua");
	controller->SetTrackingTurnSpeed(2.0f);
}

void Aracore::RegisterAIActions()
{
	// 共通ストップ
	const auto stop = [this]()
	{
		controller->StopMovement();
		StopAnimatedMovement();
	};

	// 自由行動
	const auto enterFreedomWait = [this]()
	{
		controller->StopMovement();
		StopAnimatedMovement();
		controller->SetBool("ActionFinished", false);
	};

	// 自由行動
	const auto enterWander = [this]()
	{
		const float moveSpeed = controller->GetFloat("FreedomMoveSpeed", 1.4f);
		RequestAnimatedMovement(moveSpeed, "walk");
		navMeshAgent->SetMovementPaused(false);
		navMeshAgent->SetSpeed(moveSpeed);
		// Keep the destination when returning from WanderTurn.
		if (navMeshAgent->HasDestination()) return;
		navMeshAgent->MoveToRandomPosition(
			controller->GetFloat("FreedomMinDistance", 5.0f),
			controller->GetFloat("FreedomMaxDistance", 18.0f));
	};

	// 旋回中は踏み替えアニメーションに合わせて、その場で向きを変える。
	const auto updateTurn = [this]()
	{
		const float moveSpeed = controller->GetFloat("FreedomMoveSpeed", 1.4f);
		RequestAnimatedMovement(moveSpeed, "walk");
		navMeshAgent->SetSpeed(moveSpeed);
	};

	// 旋回終了
	const auto exitTurn = [this]()
	{
		navMeshAgent->SetMovementPaused(false);
	};

	// 威嚇演出
	const auto enterThreat = [this]()
	{
		SetPlayerDetected(true);
		controller->StopMovement();
		StopAnimatedMovement();
		controller->SetBool("ActionFinished", false);
		//controller->FaceTarget();
		Actor* target = controller->GetTarget();
		if (target != threatenedTarget)
		{
			threatenedTarget = target;
			PlayThreatPresentation();
		}
	};

	// 追跡開始
	const auto enterChase = [this]()
	{
		chaseBgmEngaged = true;
		SetPlayerDetected(true);
		RequestAnimatedMovement(controller->GetFloat("ChaseMoveSpeed", 3.2f), "walk");
		controller->SetBool("ActionFinished", false);
	};

	// 追跡更新
	const auto updateChase = [this]()
	{
		if (Actor* target = controller->GetTarget())
		{
			const float moveSpeed = controller->GetFloat("ChaseMoveSpeed", 3.2f);
			RequestAnimatedMovement(moveSpeed, "walk");
			navMeshAgent->SetSpeed(moveSpeed);
			navMeshAgent->MoveToTarget(target);
		}

	};

	// 突進旋回開始
	const auto enterChargeTurn = [this]()
	{
		controller->StopMovement();
		RequestAnimatedMovement(0.0f, "turn");
		controller->SetBool("ActionFinished", false);
	};

	// 突進旋回更新
	const auto updateChargeTurn = [this]()
	{
		if (!controller->GetTarget()) return;
		RequestAnimatedMovement(0.0f, "turn");
		controller->FaceTarget();
	};

	// 突進開始
	const auto enterCharge = [this]()
	{
		controller->StopMovement();
		controller->SetBool("ActionFinished", false);
		const float chargeSpeed = controller->GetFloat("ChargeMoveSpeed", 6.5f);
		RequestAnimatedMovement(chargeSpeed, "charge");
		anim->SetTrigger("Attack");
		Actor* target = controller->GetTarget();
		if (!target) return;

		Vector3 direction = target->transform.position - this->transform.position;
		direction.y = 0.0f;
		if (direction.LengthSquared() > eps) direction.Normalize();
		else direction = this->transform.forward;

		this->transform.SetRotation(Quaternion::CreateFromYawPitchRoll(
			atan2f(direction.x, direction.z), 0.0f, 0.0f));
		const Vector3 destination = target->transform.position +
			direction * controller->GetFloat("ChargeOverDistance", 10.0f);
		navMeshAgent->SetDirectMovement(true);
		navMeshAgent->SetSpeed(chargeSpeed);
		navMeshAgent->MoveToPosition(destination);
	};

	// 突進更新
	const auto updateCharge = [this]()
	{
		RequestAnimatedMovement(controller->GetFloat("ChargeMoveSpeed", 6.5f), "charge");

	};

	// 回復開始
	const auto enterRecovery = [this]()
	{

		controller->StopMovement();
		StopAnimatedMovement();
		controller->SetBool("ActionFinished", false);

	};

	// ジャンプ開始
	const auto enterJump = [&]()
	{
		controller->StopMovement();
		StopAnimatedMovement();
		controller->SetBool("ActionFinished", false);
		jumpStartPosition = this->transform.position;
		jumpLandingPosition = this->player ? this->player->transform.position : jumpStartPosition;
		jumpLanded = false;
		controller->SetBool("JumpLanded", false);
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
	const auto updateJump = [&]()
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
		controller->SetBool("JumpLanded", jumpLanded);
	};

	// ジャンプ終了
	const auto exitJump = [&]()
	{
		jumpAnimationWaitingForLanding = false;
		characterController->SetPosition(jumpLandingPosition);
		characterController->SetUseGravity(true);
	};

	// 形態変化の演出中は、Threatの開始と演出時間の終了を待つ。
	const auto enterPhaseThreat = [this]()
	{
		BeginPhaseChange();
	};
	const auto updatePhaseThreat = [this]()
	{
		controller->SetBool("PhasePresentationStarted", !phasePresentationPending);
		controller->SetFloat("PhaseThreatElapsed", phaseThreatElapsed);
	};
	const auto exitPhaseThreat = [this, stop]()
	{
		phaseThreatActive = false;
		stop();
	};

	controller->AddAction("FreedomWait", enterFreedomWait);
	controller->AddAction("FreedomWander", enterWander);
	controller->AddAction("WanderTurn", {}, updateTurn, {}, exitTurn);
	controller->AddAction("Threat", enterThreat, {}, {}, stop);
	controller->AddAction("PhaseThreat", enterPhaseThreat, updatePhaseThreat, {}, exitPhaseThreat);
	controller->AddAction("Chase", enterChase, updateChase, {}, stop);
	controller->AddAction("ChargeTurn", enterChargeTurn, updateChargeTurn, {}, stop);
	controller->AddAction("Charge", enterCharge, updateCharge, {}, stop);
	controller->AddAction("Recovery", enterRecovery, {}, {}, stop);
	controller->AddAction("JumpCenter", enterJump, updateJump, {}, exitJump);

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
	UpdateTurningAnimation();
}

void Aracore::StopAnimatedMovement()
{
	requestedAnimationMoveSpeed = 0.0f;
	turningDirection = 0;
	requiredMovementAnimation.clear();
	movementAnimationGateActive = false;
	if (anim)
	{
		anim->SetFloat("speed", 0.0f);
		anim->SetBool("turnLeft", false);
		anim->SetBool("turnRight", false);
	}
	controller->SetBool("RotationPaused", false);
	if (navMeshAgent)
	{
		navMeshAgent->SetDirectMovement(false);
		navMeshAgent->SetMovementPaused(false);
		navMeshAgent->SetRotationPaused(false);
	}
}

void Aracore::UpdateTurningAnimation()
{
	navMeshAgent->SetAngularSpeed(controller->GetFloat("TurnAngularSpeed", 45.0f));
	const float angle = requiredMovementAnimation == "turn"
		? controller->GetFloat("TargetTurnAngle", controller->GetFloat("TurnAngle"))
		: controller->GetFloat("TurnAngle");
	const bool canTurn = requiredMovementAnimation == "walk" || requiredMovementAnimation == "turn";
	const float startThreshold = std::max(controller->GetFloat("TurnThreshold", 8.0f), 0.1f);
	const float stopThreshold = std::min(startThreshold * 0.5f, 5.0f);
	if (!canTurn || fabsf(angle) <= stopThreshold) turningDirection = 0;
	else if ((!turningDirection || turningDirection * angle < 0.0f) && fabsf(angle) > startThreshold)
		turningDirection = angle > 0.0f ? 1 : -1;
	const bool turning = turningDirection != 0;
	controller->SetBool("IsTurning", turning);
	anim->SetBool("turnLeft", turningDirection < 0);
	anim->SetBool("turnRight", turningDirection > 0);
	if (multiLegFootIK)
		multiLegFootIK->SetPreventGroundPenetration(turning ||
			anim->GetCurrentStateName().starts_with("turn_") || anim->GetNextStateName().starts_with("turn_"));
	const bool ready = IsMovementAnimationReady();
	const bool rotationPaused = requiredMovementAnimation == "charge" || (!ready && !turning);
	controller->SetBool("RotationPaused", rotationPaused);
	navMeshAgent->SetRotationPaused(rotationPaused);
	navMeshAgent->SetMovementPaused(turning || requiredMovementAnimation == "turn" || !ready);
}

bool Aracore::IsMovementAnimationReady() const
{
	if (!movementAnimationGateActive || !anim) return true;
	if (anim->IsTransitioning()) return false;

	const std::string& state = anim->GetCurrentStateName();
	if (requiredMovementAnimation == "charge")
		return state == "attack" || state == "run";
	if (requiredMovementAnimation == "walk" || requiredMovementAnimation == "turn")
	{
		if (turningDirection)
			return state == (turningDirection > 0 ? "turn_right" : "turn_left");
	}
	if (requiredMovementAnimation == "turn") return state == "Idle";
	if (requiredMovementAnimation == "walk") return state == "walk" || state == "run";
	return state == requiredMovementAnimation;
}

// HP半分で一度だけ演出と攻撃パターンを切り替える。
void Aracore::BeginPhaseChange()
{
	angry = true;
	jumpAnimationPending = false;
	phaseThreatActive = true;
	phasePresentationPending = true;
	phaseThreatElapsed = 0.0f;

	damageAnimationPending = false;
	controller->SetBool("PhaseChangeRequested", false);
	controller->SetBool("ActionFinished", false);
	controller->StopMovement();
	StopAnimatedMovement();
	controller->SetBool("Angry", true);

	if (anim->GetLayerCount() > 0 && anim->GetCurrentStateName() == "Threat")
		anim->GetLayer(0).currentTime = 0.0f;
	PlayThreatPresentation();
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

	// Threatが始まった時点から演出時間を進める。
	if (angry && !phasePresentationPending && phaseThreatActive)
		phaseThreatElapsed += deltaTime;

	UpdateHealthDissolve(deltaTime);
	UpdateAngryColor();
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

void Aracore::UpdateAngryColor()
{
	if (!angry) return;

	const Color angryBaseColor(0.7f, 0.7f, 0.7f, 1.0f);
	const Color angryEmisColor(1.0f, 0.0f, 0.0f, 0.056f);
	const float deltaTime = std::max(Game::Time::deltaTime, 0.0f);
	const float blend = 1.0f - expf(-3.0f * deltaTime);

	// 現在の色から怒り色へ、フレーム時間に依存せず滑らかに近づける。
	for (auto& material : model->GetMaterials())
	{
		material.baseColor = Color::Lerp(material.baseColor, angryBaseColor, blend);
		material.emissiveColor = Color::Lerp(material.emissiveColor, angryEmisColor, blend);
	}
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
	controller->SetFloat("Life", GetLife());
	controller->SetFloat("MaxLife", GetMaxLife());
	controller->SetBool("CanChangePhase", !IsDead() && !jumpAnimationWaitingForLanding);
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
	UpdateTurningAnimation();
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
		voiceIdChase = sound.Play("BGM/chase", options, -1);
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
	constexpr float power = 2.0f; // 強さ
	constexpr float radius = 9.0f; // 半径
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
	// 威嚇中はダメージを軽減する
	if (state == "Threat" || 
		threatAnimationPending)
	{
		realDMG.damage *= 0.2f;
	}
	Entity::TakeDamage(realDMG);
}

void Aracore::OnCollisionEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal)
{
	if (IsDead()) return;
	if (!PhysicsComponent::IsLive(self) || !PhysicsComponent::IsLive(other)) return;
	if (!self->IsActive() || !other->IsActive()) return;
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
				.attackerCollider = self,
				.targetCollider = other,
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
	if (!PhysicsComponent::IsLive(self) || !PhysicsComponent::IsLive(other)) return;
	if (!self->IsActive() || !other->IsActive()) return;
	PushPlayer(self, other);
}

bool Aracore::PushPlayer(PhysicsComponent* self, PhysicsComponent* other)
{
	if (!self || !other || self->CompareName("Enemy")) return false;

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

void Aracore::OnTriggerEnter(
	PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal)
{
	if (IsDead()) return;
	if (!PhysicsComponent::IsLive(self) || !PhysicsComponent::IsLive(other)) return;
	if (!self->IsActive() || !other->IsActive()) return;
	PushPlayer(self, other);

	Actor* otherActor = dynamic_cast<Actor*>(other->GetOwner());
	if (!otherActor || otherActor->IsPendingDestroy()) return;

	// 自分が攻撃トリガーなら
	if (self->GetLayerId() == Layers::Get("EnemyAtk"))
	{
		if (otherActor->CompareTag("Player"))
		{
			static_cast<Entity*>(otherActor)
				->TakeDamage({.damage = 10.0f,
					.knockBackPower = 10.0f,
					.attackerCollider = self,
					.targetCollider = other,
					.hitPosition = point,
					.hitNormal = normal});
		}
		else if (auto crystal = dynamic_cast<Crystal*>(otherActor))
		{
			crystal->TakeDamage({.damage = 9999.0f,
				.attackerCollider = self, .targetCollider = other,
				.hitPosition = point, .hitNormal = normal});
		}
		else if (auto deer = dynamic_cast<Deer*>(otherActor))
		{
			deer->TakeDamage({.damage = 9999.0f, .isForceDamage = true,
				.attackerCollider = self, .targetCollider = other,
				.hitPosition = point, .hitNormal = normal});
		}
		return;
	}
}

void Aracore::OnDamaged(const DamageData& damageData)
{
	Actor* attacker = damageData.attackerCollider
		? dynamic_cast<Actor*>(damageData.attackerCollider->GetOwner())
		: nullptr;
	float timeStopTime = 0.07f;
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
		// sp攻撃ならダメージアニメーション待機しつつhitstopを長くする
		if (damageData.specialAttack && !phaseThreatActive)
		{
			damageAnimationPending = true;
			timeStopTime *= 1.5f;
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
	phasePresentationPending = false;
	phaseThreatActive = false;

	controller->SetBool("PhaseChangeRequested", false);
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
