#include <ProfileKit/ProfileKit.h>
#include <ImGuiToolKit/Widgets.h>
#include <ImGuiToolKit/Drag.h>
#include "Gameplay/Actor/Aracore.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <imgui.h>

#include "Animation/Animator.h"
#include "Animation/MultiLegFootIK.h"
#include "Application/Time/GameTime.h"
#include "Audio/SoundSystem.h"

#include "Gameplay/AI/EnemyAI.h"
#include "Gameplay/Actor/ActorManager.h"
#include "Gameplay/Actor/Crystal.h"
#include "Gameplay/Actor/CrystalPart.h"
#include "Gameplay/Actor/Deer.h"
#include "Gameplay/Actor/Spawner.h"
#include "Gameplay/Camera/ThirdPersonCameraController.h"
#include "Gameplay/Player/Player.h"
#include "Gameplay/Scene/CameraEffectController.h"
#include "Gameplay/Component/ActorHitstopComponent.h"
#include "Gameplay/Stage/Component/StageLoader.h"
#include "Gameplay/Stage/Component/Terrain.h"

#include "Physics/Collider/CharacterController.h"
#include "Physics/Collider/VMDLColliderComponent.h"
#include "Physics/Core/PhysicsComponent.h"
#include "Physics/Navigation/NavMeshAgent.h"
#include "Rendering/Component/VMDL.h"
#include "Rendering/Component/VMDLModelComponent.h"
#include "Resource/ResourceManager.h"
#include "Resource/VMDLModel.h"
#include "UI/BossBarWidget.h"
#include "Rendering/Component/HitstopVibrationComponent.h"
#include "Rendering/Component/AfterimageComponent.h"
#include "Rendering/Effect/EffectManager.h"
#include "Rendering/Effect/Effect.h"

// 初期化

Aracore::Aracore(Player* player_init,
	const Transform& transform, Terrain* terrain_init, const std::string& modelPath)
	: Entity("Aracore", "Enemy", true, transform, 3000.0f, 3000.0f),
	  modelPath(modelPath), player(player_init), terrain(terrain_init), spawnPosition(transform.position)
{}

void Aracore::OnAwake()
{
	// モデル
	vmdl = AddComponent<VMDL>(modelPath);
	vmdl->Awake();
	vmdl->SetAutoUpdateTransform(false);
	vmdl->SetModelYawOffset(DirectX::XM_PI); // 反転
	model = vmdl->GetSharedModel();
	vmdl->UpdateTransform(transform.GetMatrix());

	// HitstopVibrationComponent
	hitstopVibration = AddComponent<HitstopVibrationComponent>(model);
	AddComponent<ActorHitstopComponent>(model);

	// 残像だ
	afterImage = AddComponent<AfterimageComponent>(model);

	// アニメーション。
	anim = AddComponent<Animator>(model);
	vmdl->BuildFootIK(anim);

	// 旋回はNavMesh側で制御し、威嚇・死亡時の胴体の傾きはモデルへ残す。
	anim->SetRootMotion("Box01", false);
	anim->Load("Aracore");
	anim->BindCallbacks();

	// 移動用コライダー。
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

	// 移動と足の接地補正。
	navMeshAgent = AddComponent<NavMeshAgent>();
	navMeshAgent->SetTurnSpeed(1.8f);

	multiLegFootIK = vmdl->GetMultiLegFootIK();

	ConfigureAI();
	navigation.Initialize(controller, navMeshAgent, anim, multiLegFootIK);
	RegisterAIActions();
}

Aracore::~Aracore()
{
	Uninit();
}

void Aracore::OnUninit()
{
	if (bossBar) bossBar->Hide(this);
	chasingPlayer = false;
}

void Aracore::ConfigureAI()
{
	controller = AddComponent<EnemyAI>("Aracore.lua");
	controller->SetTrackingTurnSpeed(2.0f);
	controller->WaitForCamera(Vector3(0.0f, 2.0f, 0.0f), 3.0f);
}

void Aracore::RegisterAIActions()
{
	// 行動名、開始、更新、後半更新、終了。判断条件は Aracore.lua に置く。
	controller->AddAction("FreedomWait", [this] { BeginFreedomWait(); });
	controller->AddAction("FreedomWander", [this] { BeginWander(); });
	controller->AddAction(
		"WanderTurn",
		{},
		[this] { UpdateWanderTurn(); },
		{},
		[this] { EndWanderTurn(); });
	controller->AddAction("Threat", [this] { BeginThreat(); }, {}, {}, [this] { StopAIAction(); });
	controller->AddAction(
		"PhaseThreat",
		[this] { BeginPhaseThreat(); },
		[this] { UpdatePhaseThreat(); },
		{},
		[this] { EndPhaseThreat(); });
	controller->AddAction(
		"Chase",
		[this] { BeginChase(); },
		[this] { UpdateChase(); },
		{},
		[this] { StopAIAction(); });
	controller->AddAction(
		"TossinTurn",
		[this] { BeginTossinTurn(); },
		[this] { UpdateTossinTurn(); },
		{},
		[this] { StopAIAction(); });
	controller->AddAction(
		"Tossin",
		[this] { BeginTossin(); },
		[this] { UpdateTossin(); },
		{},
		[this] { StopAIAction(); });
	controller->AddAction("Recovery", [this] { BeginRecovery(); }, {}, {}, [this] { StopAIAction(); });
	controller->AddAction(
		"JumpCenter",
		[this] { BeginJump(); },
		[this] { UpdateJump(); },
		{},
		[this] { EndJump(); });
}

// フレーム更新

void Aracore::OnUpdate()
{
	PROFILE_FUNCTION();
	START_PROFILER("Aracore::OnUpdate");
	if (movieDefeated) return;
	controller->SetFloat("Life", GetLife());
	controller->SetFloat("MaxLife", GetMaxLife());
	controller->SetBool("CanChangePhase", !IsDead() && !jump.IsWaitingForLanding());
	UpdateJumpAnimationLandingHold();

	if (jump.IsDeformPending())
	{
		jump.SetDeformPending(false);
		DeformTerrainAtLanding();
	}

	Entity::OnUpdate();

	// 自動回復
	if (life > 0.0f && healTime.Tick()) Heal(50.0f);

	// 状態変化
	UpdateHealthPresentation();

	if (death.IsReturning())
	{
		UpdateDeathSequence();
		return;
	}

	if (playerDetected)
	{
		battleTime += Game::Time::deltaTime;
	}

	navigation.Update();

	END_PROFILER();
}

void Aracore::OnLateUpdate()
{
	START_PROFILER("Aracore::OnLateUpdate");
	if (movieDefeated)
	{
		UpdateAnimatedModelTransform();
		return;
	}
	if (controller->IsWaitingForCamera() && !IsDead())
	{
		UpdateAnimatedModelTransform();
		return;
	}
	if (!IsDead()) UpdateThreatPresentation();
	UpdateReactionAnimation();
	UpdateChaseState();

	if (!IsDead() && anim->IsStateActive("just_dodge"))
	{
		characterController->Move(Vector3::Transform(anim->GetRootMotionVec(), transform.rotation));
	}

	UpdateAnimatedModelTransform();
	END_PROFILER();
}

// Luaが呼び出す行動：待機・徘徊・追跡・突進・ジャンプ・形態変化

void Aracore::StopAIAction()
{
	controller->StopMovement();
	navigation.Stop();
}

void Aracore::BeginFreedomWait()
{
	controller->StopMovement();
	navigation.Stop();
	controller->SetBool("ActionFinished", false);
}

void Aracore::BeginWander()
{
	const float moveSpeed = controller->GetFloat("FreedomMoveSpeed", 1.4f);
	navigation.Walk(moveSpeed);
	navMeshAgent->SetMovementPaused(false);
	navMeshAgent->SetSpeed(moveSpeed);
	// 旋回から戻ったときは、既存の目的地への移動を再開する。
	if (navMeshAgent->HasDestination()) return;
	navMeshAgent->MoveToRandomPosition(
		controller->GetFloat("FreedomMinDistance", 5.0f),
		controller->GetFloat("FreedomMaxDistance", 18.0f));
}

void Aracore::UpdateWanderTurn()
{
	const float moveSpeed = controller->GetFloat("FreedomMoveSpeed", 1.4f);
	navigation.Walk(moveSpeed);
	navMeshAgent->SetSpeed(moveSpeed);
}

void Aracore::EndWanderTurn()
{
	navMeshAgent->SetMovementPaused(false);
}

void Aracore::BeginThreat()
{
	SetPlayerDetected(true);
	controller->StopMovement();
	navigation.Stop();
	controller->SetBool("ActionFinished", false);
	//controller->FaceTarget();
	Actor* target = controller->GetTarget();
	if (target != reaction.GetLastThreatTarget())
	{
		reaction.SetLastThreatTarget(target);
		PlayThreatPresentation();
	}
}

void Aracore::BeginChase()
{
	chasingPlayer = true;
	SetPlayerDetected(true);
	navigation.Chase(controller->GetFloat("ChaseMoveSpeed", 3.2f));
	controller->SetBool("ActionFinished", false);
}

void Aracore::UpdateChase()
{
	if (Actor* target = controller->GetTarget())
	{
		const float moveSpeed = controller->GetFloat("ChaseMoveSpeed", 3.2f);
		navigation.Chase(moveSpeed);
		navMeshAgent->SetSpeed(moveSpeed);
		navMeshAgent->MoveToTarget(target);
	}
}

void Aracore::BeginTossinTurn()
{
	controller->StopMovement();
	navigation.TurnInPlace();
	controller->SetBool("ActionFinished", false);
}

void Aracore::UpdateTossinTurn()
{
	if (!controller->GetTarget()) return;
	navigation.TurnInPlace();
	controller->FaceTarget();
}

void Aracore::BeginTossin()
{
	controller->StopMovement();
	controller->SetBool("ActionFinished", false);
	const float tossinSpeed =
		controller->GetFloat(phase.IsAngry() ? "AngryTossinMoveSpeed" : "TossinMoveSpeed", 6.5f);

	Actor* target = controller->GetTarget();
	if (!target)
	{
		anim->SetInt("attackRand", 1); // 0 or 1
		anim->SetTrigger("Attack");
		return;
	}

	Vector3 direction = target->transform.position - this->transform.position;
	direction.y = 0.0f;
	float distSq = direction.LengthSquared();

	if (distSq > eps) direction.Normalize();
	else direction = this->transform.GetForward();

	int attackRand = distSq <= 20.0f ? 0 : 1; // 0 or 1
	anim->SetInt("attackRand", attackRand);
	anim->SetTrigger("Attack");
	// attackはattack2の速度40％で前進(少しは前進しないと攻撃＝＞回避＝＞攻撃の無限ループになるため)
	const float moveSpeed = attackRand ? tossinSpeed : tossinSpeed * 0.4f;
	navigation.Tossin(moveSpeed);

	this->transform.SetRotation(
		Quaternion::CreateFromYawPitchRoll(atan2f(direction.x, direction.z), 0.0f, 0.0f));
	const Vector3 destination =
		target->transform.position + direction * controller->GetFloat("TossinOverDistance", 10.0f);
	navMeshAgent->SetDirectMovement(true);
	navMeshAgent->SetSpeed(moveSpeed);
	navMeshAgent->MoveToPosition(destination);
}

void Aracore::UpdateTossin()
{
	const float tossinSpeed =
		controller->GetFloat(phase.IsAngry() ? "AngryTossinMoveSpeed" : "TossinMoveSpeed", 6.5f);
	const float moveSpeed = anim->GetInt("attackRand") ? tossinSpeed : tossinSpeed * 0.4f;
	navigation.Tossin(moveSpeed);
	navMeshAgent->SetSpeed(moveSpeed);
}

void Aracore::BeginRecovery()
{

	controller->StopMovement();
	navigation.Stop();
	controller->SetBool("ActionFinished", false);
}

void Aracore::BeginJump()
{
	controller->StopMovement();
	navigation.Stop();
	controller->SetBool("ActionFinished", false);
	jump.Begin(transform.position, player ? player->transform.position : transform.position);
	controller->SetBool("JumpLanded", false);
	characterController->SetUseGravity(false);
	if (anim->GetCurrentStateName() == "Jump" && anim->GetNextStateName().empty())
	{
		anim->SetCurrentAnimationTime(0.0f);
		anim->SetCurrentAnimationSpeed(1.0f);
		jump.SetAnimationPending(false);
	}
	else anim->SetTrigger("Jump");
}

void Aracore::UpdateJump()
{
	PROFILE_FUNCTION();
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
	Vector3 position = Vector3::Lerp(jump.GetStartPosition(), jump.GetLandingPosition(), t);
	position.y += heightOffset;
	characterController->SetPosition(position);
	if (!jump.IsLanded() && t >= 1.0f)
	{
		jump.Landed();
		characterController->SetUseGravity(true);
		SpawnDeerFromSky();
		ShakeCameraAtLanding();
		jump.SetDeformPending(terrain != nullptr);
	}
	controller->SetBool("JumpLanded", jump.IsLanded());
}

void Aracore::EndJump()
{
	jump.SetWaitingForLanding(false);
	characterController->SetPosition(jump.GetLandingPosition());
	characterController->SetUseGravity(true);
}

void Aracore::BeginPhaseThreat()
{
	BeginPhaseChange();
}

void Aracore::UpdatePhaseThreat()
{
	controller->SetBool("PhasePresentationStarted", !phase.IsWaitingForAnimation());
	controller->SetFloat("PhaseThreatElapsed", phase.GetElapsedTime());
}

void Aracore::EndPhaseThreat()
{
	phase.EndThreat();
	StopAIAction();
}

// アニメーション要求と着地待ち

void Aracore::PlayThreatPresentation()
{
	reaction.RequestThreat();
}

void Aracore::UpdateThreatPresentation()
{
	if (!controller || phase.IsThreatActive()) return;

	Actor* target = controller->GetTarget();
	if (!target)
	{
		reaction.SetLastThreatTarget(nullptr);
		reaction.SetThreatPending(false);
		SetPlayerDetected(false);
		return;
	}
	// 距離検索で候補に入っただけでは発見扱いにしない。視野角と遮蔽物判定を通過してから威嚇する。
	if (!controller->GetBool("IsTargetVisible")) return;
	SetPlayerDetected(true);
	if (target == reaction.GetLastThreatTarget()) return;

	reaction.SetLastThreatTarget(target);
	PlayThreatPresentation();
}

void Aracore::UpdateReactionAnimation()
{
	PROFILE_FUNCTION();
	if (!anim || IsDead()) return;
	const auto& current = anim->GetCurrentStateName();
	const auto& next = anim->GetNextStateName();

	// 被ダメージを優先し、未受理の要求だけを再送する。
	if (reaction.IsDamagePending())
	{
		if (anim->IsStateActive("damage")) reaction.SetDamagePending(false);
		else
		{
			anim->SetTrigger("Damage");
			return;
		}
	}

	if (jump.IsAnimationPending() && !phase.IsThreatActive())
	{
		if (next == "Jump" || (current == "Jump" && next.empty())) jump.SetAnimationPending(false);
		else
		{
			anim->SetTrigger("Jump");
			return;
		}
	}

	if (!reaction.IsThreatPending()) return;

	if (anim->IsStateActive("Threat"))
	{
		reaction.SetThreatPending(false);
		if (phase.IsWaitingForAnimation())
		{
			SoundSystem::Instance().Play("SE/Threat");
			phase.AnimationStarted();
			CrystalPart::Spawn(transform.position + Vector3(0.0f, 3.0f, 0.0f),
				{0.0f, 7.0f, 0.0f}, 6, 2.5f, 7.0f, 2.0f, this);
			vmdl->GetRenderer()->PlaySoundSource("DAMAGED");
			CameraEffectController::Request(0.5f, 0.15f);
		}
		return;
	}

	if (anim->IsStateActive("damage")) return;
	anim->SetTrigger("Threat");
}

void Aracore::UpdateJumpAnimationLandingHold()
{
	if (!anim) return;
	anim->SetStateTimeHold("Jump", 1.082f, jump.IsWaitingForLanding() || death.IsReturning());
}

void Aracore::UpdateAnimatedModelTransform()
{
	if (!model || !vmdl) return;

	vmdl->UpdateTransform(transform.GetMatrix());
}

// 形態変化・体力に応じた見た目

void Aracore::BeginPhaseChange()
{
	phase.Begin();
	jump.SetAnimationPending(false);

	reaction.SetDamagePending(false);
	controller->SetBool("PhaseChangeRequested", false);
	controller->SetBool("ActionFinished", false);
	controller->StopMovement();
	navigation.Stop();
	controller->SetBool("Angry", true);

	if (anim->GetCurrentStateName() == "Threat")
		anim->SetCurrentAnimationTime(0.0f);
	PlayThreatPresentation();
}

void Aracore::UpdateHealthPresentation()
{
	const float deltaTime = std::max(Game::Time::deltaTime, 0.0f);

	// Threatが始まった時点から演出時間を進める。
	phase.Advance(deltaTime);

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

	const bool deathAnimationReady = IsDead() && death.IsAnimationPlaying();
	const float difference = std::abs(healthDissolveAmount - targetDissolveAmount);
	if (deathAnimationReady || difference < 0.0001f)
		healthDissolveAmount = targetDissolveAmount;

	auto& params = vmdl->GetRenderer()->GetRenderParams();
	params.dissolveAmount = std::clamp(healthDissolveAmount, 0.0f, 1.0f);
}

void Aracore::UpdateAngryColor()
{
	if (!phase.IsAngry()) return;

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

// 着地時の召喚・地形・カメラ演出

void Aracore::SpawnDeerFromSky()
{
	if (!stageLoader) return;

	ActorManager* actorManager = ActorManager::GetActive();
	if (!actorManager) return;

	// 倒された召喚鹿は同時出現数に含めない。
	auto deers = actorManager->GetActorsByName("BossFieldDeer");

	constexpr float spawnHeight = 20.0f;
	constexpr size_t maxBossSummonedDeer = 4; // 召喚最大数
	for (Spawner* spawner : stageLoader->GetSpawners("Deer"))
	{
		if (deers.size() >= maxBossSummonedDeer) break;
		if (std::rand() % 3 != 0) continue;

		Transform spawnTransform = spawner->GetSummonTransform();
		spawnTransform.position.y += spawnHeight;
		Actor* summonedDeer = spawner->Summon(spawnTransform);
	}
}

void Aracore::DeformTerrainAtLanding()
{
	if (!terrain || !controller) return;
	constexpr float power = 2.0f; // 強さ
	constexpr float radius = 9.0f; // 半径
	terrain->Deform(jump.GetLandingPosition(), Vector3::Down, power, radius);
}

void Aracore::ShakeCameraAtLanding()
{
	if (!player) return;

	constexpr float fullShakeDistance = 6.0f;
	constexpr float noShakeDistance = 45.0f;
	constexpr float maximumDuration = 0.55f;
	constexpr float maximumIntensity = 0.50f;

	const float distance = (player->transform.position - jump.GetLandingPosition()).Length();
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

// ダメージと接触

void Aracore::TakeDamage(const DamageData& damageData)
{
	if (movieDefeated || defeatMovieRequested) return;

	// 体力半分になって覚醒したらジャスト回避による必殺技しか効かなくする
	if (phase.IsAngry())
	{
		if (!damageData.specialAttack)
		{
			afterImage->Play(0.5f);
			anim->SetTrigger("JustDodge");
			vmdl->GetRenderer()->PlayParticleEmitter("GUARD");
			return;
		}
	}

	if (life <= 0.0f) return;

	DamageData realDMG = damageData;
	const std::string& state = anim->GetCurrentStateName();

	// 威嚇中はダメージを軽減する
	if (state == "Threat" || reaction.IsThreatPending()) realDMG.damage *= 0.2f;

	Entity::TakeDamage(realDMG);
}

void Aracore::OnCollisionEnter(
	PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal)
{
	if (IsDead()) return;
	if (!PhysicsComponent::IsLive(self) || !PhysicsComponent::IsLive(other)) return;
	if (!self->IsActive() || !other->IsActive()) return;
	PushPlayer(self, other);

	Actor* otherActor = dynamic_cast<Actor*>(other->GetOwner());
	if (!otherActor) return;

	// クリスタルは問答無用でぶっこわす(進行の邪魔)
	if (otherActor->CompareTag("Crystal"))
	{
		if (auto crystal = dynamic_cast<Crystal*>(otherActor))
		{
			crystal->TakeDamage({.damage = 9999.0f,
				.attackerCollider = self,
				.targetCollider = other,
				.hitPosition = point,
				.hitNormal = normal});
		}
	}

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

void Aracore::OnCollisionStay(
	PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal)
{
	if (IsDead()) return;
	if (!PhysicsComponent::IsLive(self) || !PhysicsComponent::IsLive(other)) return;
	if (!self->IsActive() || !other->IsActive()) return;
	PushPlayer(self, other);
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

	// クリスタルは問答無用でぶっこわす(進行の邪魔)
	if (otherActor->CompareTag("Crystal"))
	{
		if (auto crystal = dynamic_cast<Crystal*>(otherActor))
		{
			crystal->TakeDamage({.damage = crystal->GetLife(),
				.attackerCollider = self,
				.targetCollider = other,
				.hitPosition = point,
				.hitNormal = normal});
		}
	}

	// 自分が攻撃トリガーなら
	if (self->GetLayerId() == Layers::Get("EnemyAtk"))
	{
		if (otherActor->CompareTag("Player"))
		{
			static_cast<Entity*>(otherActor)
				->TakeDamage({.damage = 10.0f,
					.knockBackPower = 15.0f,
					.attackerCollider = self,
					.targetCollider = other,
					.hitPosition = point,
					.hitNormal = normal});
		}
		else if (auto deer = dynamic_cast<Deer*>(otherActor))
		{
			deer->TakeDamage({.damage = deer->GetLife(), .isForceDamage = true,
				.attackerCollider = self, .targetCollider = other,
				.hitPosition = point, .hitNormal = normal});
		}
		return;
	}
}

bool Aracore::PushPlayer(PhysicsComponent* self, PhysicsComponent* other)
{
	if (!self || !other || self->CompareName("Enemy")) return false;

	Entity* player = dynamic_cast<Entity*>(other->GetOwner());
	if (!player || !player->CompareTag("Player")) return false;

	Vector3 direction = player->transform.position - transform.position;
	direction.y = 0.0f;
	if (direction.LengthSquared() <= eps) direction = transform.GetForward();
	direction.Normalize();

	constexpr float pushSpeed = 10.0f;
	const float currentSpeed = player->GetKnockBackVelocity().Dot(direction);
	if (currentSpeed < pushSpeed)
		player->AddKnockBack(direction * (pushSpeed - currentSpeed));
	return true;
}

void Aracore::OnDamaged(const DamageData& damageData)
{
	Actor* attacker = damageData.attackerCollider
		? dynamic_cast<Actor*>(damageData.attackerCollider->GetOwner())
		: nullptr;
	float timeStopTime = 0.07f;
	if (attacker && attacker->CompareTag("Player"))
	{
		healTime.Reset();

		const bool newlyDetected =
			!playerDetected || attacker != reaction.GetLastThreatTarget();
		if (controller) controller->LockOn(attacker);
		reaction.SetLastThreatTarget(attacker);
		SetPlayerDetected(true);
		if (newlyDetected)
		{
			PlayThreatPresentation();
		}
		// sp攻撃ならダメージアニメーション待機しつつhitstopを長くする
		if (damageData.specialAttack && !phase.IsThreatActive())
		{
			reaction.RequestDamage();
			timeStopTime *= 1.5f;
		}
	}
	GetComponent<ActorHitstopComponent>()->Request(timeStopTime);
	CameraEffectController::Request(0.2f, 0.1f);
	hitstopVibration->HitStop(0.22f, 0.15f);

	// ダメージサウンド
	vmdl->GetRenderer()->PlaySoundSource("DAMAGED");
}

// 死亡演出

void Aracore::RequestDefeatMovie()
{
	if (movieDefeated || defeatMovieRequested || life > 0.0f) return;
	defeatMovieRequested = true;
}

void Aracore::CompleteDefeatMovie()
{
	if (movieDefeated) return;
	movieDefeated = true;
	defeatMovieRequested = false;
	life = 0.0f;
	OnDead({});
}

void Aracore::OnDead(const DamageData& damageData)
{
	jump.SetAnimationPending(false);
	phase.CancelPresentation();

	controller->SetBool("PhaseChangeRequested", false);
	reaction.ClearPending();
	SetPlayerDetected(false);
	chasingPlayer = false;
	if (controller)
	{
		controller->StopMovement();
		controller->SetActive(false);
	}
	navigation.Stop();
	if (navMeshAgent) navMeshAgent->SetMovementPaused(true);

	jump.SetWaitingForLanding(false);
	knockBackVelocity = Vector3::Zero;
	if (characterController) characterController->SetActive(false);
	if (anim)
	{
		anim->SetBool("Dead", true);
		anim->SetActive(true);
	}

	if (player)
		if (auto* cameraController = player->GetCameraController())
			cameraController->RequestBossDefeatFocus(this, 2.4f);
	SoundSystem::Instance().Play("SE/Dead");

	// 飛び散りますの
	CrystalPart::Spawn(transform.position + Vector3(0.0f, 10.0f, 0.0f),
		{0.0f, 7.0f, 0.0f}, 10, 2.5f, 7.0f, 2.0f, this);
}

void Aracore::UpdateDeathSequence()
{
	if (anim && jump.IsAnimationPending())
	{
		const auto& current = anim->GetCurrentStateName();
		const auto& next = anim->GetNextStateName();
		if (next == "Jump" || (current == "Jump" && next.empty())) jump.SetAnimationPending(false);
		else anim->SetTrigger("Jump");
	}

	death.Advance(Game::Time::unscaledDeltaTime);
	const float duration = std::max(death.GetDuration(), 0.01f);
	const float flightProgress = death.GetElapsedTime() / duration;
	const float t = std::clamp(flightProgress, 0.0f, 1.0f);
	const float arcShape = 4.0f * t * (1.0f - t);
	const float heightOffset = death.GetHeight() * arcShape;

	Vector3 position = Vector3::Lerp(death.GetStartPosition(), death.GetLandingPosition(), t);
	position.y += heightOffset;
	if (characterController) characterController->SetPosition(position);
	else transform.SetPosition(position);

	if (t < 1.0f) return;
	if (death.IsAnimationPlaying()) return;
	if (characterController)
	{
		characterController->SetPosition(death.GetLandingPosition());
		characterController->SetUseGravity(true);
	}
	else transform.SetPosition(death.GetLandingPosition());

	Vector3 faceDirection = player
		? player->transform.position - death.GetLandingPosition() : transform.GetForward();
	faceDirection.y = 0.0f;
	if (faceDirection.LengthSquared() > eps)
	{
		faceDirection.Normalize();

		transform.SetRotation(Quaternion::CreateFromYawPitchRoll(
			atan2f(faceDirection.x, faceDirection.z), 0.0f, 0.0f));
	}
	ShakeCameraAtLanding();
	death.Landed();
	if (anim) anim->SetBool("Dead", true);
}

// 追跡状態とHUD

void Aracore::UpdateChaseState()
{
	if (IsDead() || !controller || !controller->GetBool("IsTargetInLostRange"))
	{
		chasingPlayer = false;
		SetPlayerDetected(false);
	}
}

void Aracore::SetBossBar(BossBarWidget* value)
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

// デバッグ表示

void Aracore::OnDrawGUI()
{
	Entity::OnDrawGUI();
	ImGuiToolKit::TextUnformatted(u8"AIパス：Resources/AI/Aracore.lua");

	ImGuiToolKit::Text(u8"バトル時間: %.2f秒", battleTime);

	if (ImGuiToolKit::Button(u8"威嚇"))
	{
		PlayThreatPresentation();
	}

	if (ImGuiToolKit::Button(u8"20ダメージ"))
	{
		TakeDamage(DamageData{.damage = 20.0f});
	}

	if (ImGui::Button("Kill"))
	{
		TakeDamage(DamageData{.damage = 10000.0f});
	}

	for (Vector3& pos : colPositions)
	{
		ImGui::PushID(&pos);
		ImGuiToolKit::DragPosFloat3(u8"Foot Collider Offset", &pos.x, 0.01f);
		ImGui::PopID();
	}
}

// 移動・旋回・アニメーションの同期

void AracoreAINavigation::Initialize(
	EnemyAI* ai, NavMeshAgent* navAgent, Animator* animation, MultiLegFootIK* footIK)
{
	controller = ai;
	agent = navAgent;
	animator = animation;
	feet = footIK;
}

void AracoreAINavigation::Request(Movement value, float moveSpeed)
{
	speed = std::max(moveSpeed, 0.0f);
	movement = value;
	Update();
}

void AracoreAINavigation::Stop()
{
	speed = 0.0f;
	turningDirection = 0;
	movement = Movement::Stopped;

	if (animator)
	{
		animator->SetFloat("speed", 0.0f);
		animator->SetBool("turnLeft", false);
		animator->SetBool("turnRight", false);
	}

	controller->SetBool("IsTurning", false);
	controller->SetBool("RotationPaused", true);
	if (agent)
	{
		agent->SetDirectMovement(false);
		agent->SetMovementPaused(true);
		agent->SetRotationPaused(true);
	}
}

void AracoreAINavigation::Update()
{
	PROFILE_FUNCTION();
	if (animator) animator->SetFloat("speed", speed);
	agent->SetAngularSpeed(controller->GetFloat("TurnAngularSpeed", 45.0f));

	const float angle = movement == Movement::TurnInPlace
		? controller->GetFloat("TargetTurnAngle", controller->GetFloat("TurnAngle"))
		: movement == Movement::Chase ? DEG(agent->GetTurnAngle()) : controller->GetFloat("TurnAngle");
	UpdateTurnDirection(angle);

	const bool turning = turningDirection != 0;
	controller->SetBool("IsTurning", turning);
	animator->SetBool("turnLeft", turningDirection < 0);
	animator->SetBool("turnRight", turningDirection > 0);

	if (feet)
		feet->SetPreventGroundPenetration(turning ||
			animator->GetCurrentStateName().starts_with("turn_") || animator->GetNextStateName().starts_with("turn_"));

	const bool ready = IsAnimationReady();
	const bool dodging = animator->IsStateActive("just_dodge");
	const bool rotationPaused = dodging || movement == Movement::Stopped ||
		movement == Movement::Tossin || (!ready && !turning);
	controller->SetBool("RotationPaused", rotationPaused);

	agent->SetRotationPaused(rotationPaused);
	agent->SetMovementPaused(dodging || movement == Movement::Stopped ||
		turning || movement == Movement::TurnInPlace || !ready);
}

void AracoreAINavigation::UpdateTurnDirection(float angle)
{
	const bool canTurn = (movement == Movement::Walk && agent && agent->HasDestination()) ||
		movement == Movement::Chase || movement == Movement::TurnInPlace;
	const float startThreshold = std::max(controller->GetFloat("TurnThreshold", 8.0f), 0.1f);
	const float stopThreshold = std::min(startThreshold * 0.5f, 5.0f);
	if (!canTurn || fabsf(angle) <= stopThreshold) turningDirection = 0;
	else if ((!turningDirection || turningDirection * angle < 0.0f) && fabsf(angle) > startThreshold)
		turningDirection = angle > 0.0f ? 1 : -1;
}

bool AracoreAINavigation::IsAnimationReady() const
{
	if (movement == Movement::Stopped || !animator) return true;
	if (animator->IsTransitioning()) return false;

	const std::string& state = animator->GetCurrentStateName();
	if (movement == Movement::Tossin)
		return speed > 0.0f && (state == "run" || state == "attack" || state == "attack2");
	if (movement == Movement::Walk || movement == Movement::Chase || movement == Movement::TurnInPlace)
	{
		if (turningDirection)
			return state == (turningDirection > 0 ? "turn_right" : "turn_left");
	}
	if (movement == Movement::TurnInPlace) return state == "Idle";
	if (movement == Movement::Walk || movement == Movement::Chase)
		return state == "walk" || state == "run";
	return false;
}
