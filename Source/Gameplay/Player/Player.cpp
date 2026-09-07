#include "Gameplay/Player/Player.h"
#include "Audio/SoundTracks.generated.h"
#include "Application/Input/Input.h"
#include "Gameplay/Camera/ThirdPersonCameraController.h"
#include "Gameplay/Scene/SceneManager.h"
#include "Gameplay/Stage/Component/Terrain.h"
#include "Application/Time/GameTime.h"
#include "Rendering/Core/Graphics.h"
#include "Gameplay/Scene/PostProcessController.h"
#include "Gameplay/Scene/CameraEffectController.h"
#include "Gameplay/Scene/TimeScaleController.h"
#include "Gameplay/Actor/ActorManager.h"
#include "Gameplay/Actor/AracoreQueen.h"
#include "Gameplay/Actor/EnemySmall.h"
#include "Gameplay/Component/CharacterMotorComponent.h"
#include "Gameplay/Component/LockOnComponent.h"
#include "Physics/Core/PhysicsManager.h"
#include "Physics/Collider/VMDLColliderComponent.h"
#include "Rendering/Component/VMDLModelComponent.h"
#include "Rendering/Component/AfterimageComponent.h"

Player::Player() : Entity("Player", "Player", true, 100.0f, 100.0f)
{
	// VMDL読み込み
	vmdl = AddComponent<VMDL>("Resources/Model/Player/CombatGirls_Sword_Shield");
	model = vmdl->GetSharedModel();
	vmdl->SetAutoUpdateTransform(false);
	vmdl->SetModelYawOffset(RAD(180.0f));

	// 足音切り替え用
	footSound = vmdl->GetSoundSource("footsound");

	// 状態遷移とゲーム固有コールバックはAnimator側で設定する
	anim = vmdl->GetAnimator();
	anim->Load("Resources/Animator/Player.animator");
	anim->BindCallbacks();

	// キャラクターコントローラ生成
	float radius = 0.25f;
	float totalHeight = 1.7f;
	float capsuleHeight = totalHeight - radius * 2.0f;

	cc = AddComponent<CharacterController>(
		Layers::Get("Player"),
		radius,
		capsuleHeight
	);
	cc->SetStepOffset(0.15f);
	cc->SetSlopeLimitDeg(70.0f);
	cc->SetContactOffset(0.05f);
	cc->SetOwnerAnchorAtCenter(false);
	cc->SetOwnerAnchorOffsetY(0.0f);

	motor = AddComponent<CharacterMotorComponent>(anim, cc);
	motor->SetRootMotionNode("root");

	lockOnComponent = AddComponent<LockOnComponent>();
	lockOnComponent->SetAcquireRange(10.0f);
	lockOnComponent->SetLostRange(20.0f);
	lockOnComponent->SetRotationSpeed(6.0f);

	// SetFootPositionとSetPositionは両方呼ばない
	cc->SetFootPosition({36.82f, 0.4f, 5.148f});

	// 注視処理
	auto lookAt = AddComponent<LookAt>(
		model.get(), "head", "neck_01");
	lookAt->SetLookDistance(10.0f);
	lookAt->SetFilterTags({"Enemy"});

	// プレイヤーはダメージのクールダウン長い
	GetCooldowns().DamageCooldownDuration = 1.5f;

	// 回避の残像には描画リソースと取得時の姿勢だけを保持する
	afterimage = AddComponent<AfterimageComponent>(model);
}

void Player::SetSpawnTransform(const Transform& spawnTransform)
{
	transform.position = spawnTransform.position;
	transform.rotation = spawnTransform.rotation;
	transform.Update();
	if (cc) cc->SetFootPosition(spawnTransform.position);
}

void Player::OnUpdate()
{
	Entity::OnUpdate();
	dodgeCooldownTimer = std::max(
		dodgeCooldownTimer - Game::Time::unscaledDeltaTime, 0.0f);

	const std::string& stateName = anim->GetCurrentStateName();
	const bool isAttacking =
		stateName.starts_with("Attack") || stateName.starts_with("SpSkill");
	const bool isHit = stateName == "Hit_RFreeze" || stateName == "Hit_LFreeze";
	lockOnComponent->SetAimActive(isAttacking);
	lockOnComponent->SetRotationPaused(isHit || IsDead());

	UpdateFootSound();
	UpdateMovement();
	if (dodgeInvincible)
		PostProcessController::Instance().RequestInvincibilityAura();
	if (motor)
	{
		motor->SetExternalVelocity(knockBackVelocity);
	}

	const bool deformKeyHeld =
		Game::Input::IsFocusedWindow() && (GetAsyncKeyState('U') & 0x8000) != 0;
	if (deformKeyHeld && !terrainDeformKeyHeld)
	{
		Scene* scene = SceneManager::Instance().GetCurrentScene();
		Stage* stage = scene ? scene->GetCurrentStage() : nullptr;
		Terrain* terrain = stage ? stage->GetComponent<Terrain>() : nullptr;
		if (terrain) terrain->Deform(transform.position, Vector3::Down, 5.0f);
	}
	terrainDeformKeyHeld = deformKeyHeld;
}

// 足元のTerrainレイヤーに合わせてVMDLの足音トラックを切り替える
void Player::UpdateFootSound()
{
	if (!footSound) return;

	Scene* scene = SceneManager::Instance().GetCurrentScene();
	Stage* stage = scene ? scene->GetCurrentStage() : nullptr;
	Terrain* terrain = stage ? stage->GetComponent<Terrain>() : nullptr;
	if (!terrain) return;

	const std::string layerName = terrain->GetSurfaceLayerName(transform.position);
	if (layerName == "grass")
	{
		footSound->track = static_cast<int>(SoundTrack::SE_PLAYER_PL_WALK_GRASS);
	}
	else if (layerName == "rock" || layerName == "stone")
	{
		footSound->track = static_cast<int>(SoundTrack::SE_PLAYER_PL_WALK_ROCK);
	}
	else
	{
		footSound->track = static_cast<int>(SoundTrack::SE_PLAYER_PL_WALK);
	}
}

void Player::OnLateUpdate()
{
	vmdl->UpdateTransform(transform.matrix);

	if (cc)
	{
		cc->ClearDebugRenderPosition();
	}
}

void Player::OnDrawGUI()
{
	Entity::OnDrawGUI();
	ImGui::Text("Dodge invincible: %s", dodgeInvincible ? "Yes" : "No");
	ImGui::Text("Just dodge: %s", justDodgeTriggered ? "Yes" : "No");
	ImGui::Text("Dodge cooldown: %.2f", dodgeCooldownTimer);
}

void Player::OnDamaged(const DamageData& damageData)
{
	CameraEffectController::Request(0.13f, 0.07f);
	float lifeIntensity = (1 - (life / maxLife)) * 0.5f;
	PostProcessController::Instance().RequestDamagedVignette(
		5.0f * lifeIntensity, 3.0f * lifeIntensity, 0.15f, Easing::Type::InSine, Easing::Type::OutCubic);

	if (damageData.hitPosition.has_value())
	{
		// 敵の位置に応じてアニメーション再生
		Vector3 dir = damageData.hitPosition.value() - transform.position;
		dir.Normalize();

		float dot = transform.right.Dot(dir);

		if (dot >= 0.0f)
		{
			anim->SetTrigger("Hit_R");
		}
		else
		{
			anim->SetTrigger("Hit_L");
		}
	}
	else
	{
		anim->SetTrigger("Hit_R");
	}
	lockOnComponent->PauseRotation(0.15f);
}

void Player::TakeDamage(const DamageData& damageData)
{
	if (dodgeInvincible)
	{
		Actor* attacker = damageData.hitColliderSelf
			? dynamic_cast<Actor*>(damageData.hitColliderSelf->GetOwner())
			: nullptr;
		if (!justDodgeSkillActive &&
			IsEnemyAttackActive(attacker, damageData.hitColliderSelf))
		{
			TriggerJustDodge();
		}
		return;
	}
	Entity::TakeDamage(damageData);
}

void Player::TriggerJustDodge()
{
	if (justDodgeTriggered) return;
	justDodgeTriggered = true;
	if (afterimage) afterimage->Play();

	CameraEffectController::Request(0.13f, 0.045f);
	CameraEffectController::RequestFovOffset(0.24f, 8.0f);
	TimeScaleController::Request(0.20f, 0.03f);
	PostProcessController::Instance().RequestJustDodge();
}

bool Player::IsEnemyAttackActive(
	const Actor* enemy, const PhysicsComponent* collider) const
{
	if (!enemy || !collider || !collider->IsActive() ||
		!enemy->IsActive() || enemy->IsPendingDestroy() ||
		!enemy->CompareTag("Enemy"))
	{
		return false;
	}

	const LayerId layer = collider->GetLayerId();
	if (layer != Layers::Get("EnemyAtk") &&
		layer != Layers::Get("AracoreAtkStamp"))
	{
		return false;
	}

	const Animator* enemyAnimator = enemy->GetComponent<Animator>();
	if (!enemyAnimator) return false;
	const std::string& state = enemyAnimator->IsTransitioning()
		? enemyAnimator->GetNextStateName()
		: enemyAnimator->GetCurrentStateName();

	if (dynamic_cast<const AracoreQueen*>(enemy))
		return state == "walk" || state == "attack" || state == "Jump";
	if (dynamic_cast<const EnemySmall*>(enemy))
		return state == "run";

	return state.find("Attack") != std::string::npos ||
		state.find("attack") != std::string::npos ||
		state.find("Charge") != std::string::npos ||
		state.find("charge") != std::string::npos;
}

bool Player::HasIncomingEnemyAttack() const
{
	ActorManager* actorManager = ActorManager::GetActive();
	if (!actorManager) return false;

	const LayerId enemyAttackLayer = Layers::Get("EnemyAtk");
	const LayerId aracoreStampLayer = Layers::Get("AracoreAtkStamp");
	constexpr float justDodgeThreatRange = 4.0f;
	constexpr float justDodgeThreatRangeSq =
		justDodgeThreatRange * justDodgeThreatRange;

	for (Actor* enemy : actorManager->GetActors())
	{
		if (!enemy || enemy == this || !enemy->IsActive() ||
			enemy->IsPendingDestroy() || !enemy->CompareTag("Enemy"))
		{
			continue;
		}

		const Entity* entity = dynamic_cast<const Entity*>(enemy);
		if (entity && entity->IsDead()) continue;

		for (PhysicsComponent* collider : enemy->GetComponents<PhysicsComponent>())
		{
			if (!collider ||
				(collider->GetLayerId() != enemyAttackLayer &&
				 collider->GetLayerId() != aracoreStampLayer) ||
				!IsEnemyAttackActive(enemy, collider))
			{
				continue;
			}

			Vector3 attackPosition = enemy->transform.position;
			if (const auto* attachment =
				dynamic_cast<const VMDLColliderComponent*>(collider))
			{
				attackPosition = attachment->GetWorldPosition();
			}

			Vector3 toAttack = attackPosition - transform.position;
			toAttack.y = 0.0f;
			if (toAttack.LengthSquared() <= justDodgeThreatRangeSq) return true;
		}
	}

	return false;
}

void Player::OnDead(const DamageData& damageData)
{
	lockOnComponent->ClearTarget();
	lockOnComponent->SetActive(false);
}

void Player::OnCollisionEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal)
{
	//printf("OnCollisionEnter: %s\n", other->GetName().c_str());
}

void Player::OnTriggerEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal)
{
	if (!self || !other) return;
	if (!self->IsActive()) return;
	if (self->GetLayerId() != Layers::Get("PlayerAtk")) return;

	const bool footAtk = self->CompareName("kick");

	// 敵を殴る

	Actor* otherActor = dynamic_cast<Actor*>(other->GetOwner());
	if (!otherActor) return;
	if (!otherActor->CompareTag("Enemy") && !otherActor->CompareTag("CrystalProp")) return;
	Entity* entity = dynamic_cast<Entity*>(otherActor);
	if (!entity) return;
	const bool skillAttack =
		justDodgeSkillActive ||
		(anim && (anim->GetCurrentStateName().starts_with("SpSkill") ||
			anim->GetNextStateName().starts_with("SpSkill")));
	if (skillAttack && justDodgeSkillHitActors.contains(otherActor)) return;
	if (skillAttack) justDodgeSkillHitActors.insert(otherActor);

	Vector3 hitPosition = point;
	Vector3 hitNormal = normal;
	VMDLColliderComponent* attackCollider = dynamic_cast<VMDLColliderComponent*>(self);
	if (!attackCollider) return;
	Vector3 rayOrigin = attackCollider->GetWorldPosition();
	Vector3 rayDirection = point - rayOrigin;
	if (rayDirection.LengthSquared() <= eps)
		rayDirection = otherActor->transform.position - rayOrigin;
	if (rayDirection.LengthSquared() > eps)
	{
		float rayDistance = rayDirection.Length() * 2.0f + 2.0f;
		rayDirection.Normalize();

		PhysicsManager::PhysicsRaycastHit hit;
		if (PhysicsManager::Instance().Raycast(
			rayOrigin,
			rayDirection,
			rayDistance,
			hit,
			self->GetLayerId(),
			this))
		{
			hitPosition = hit.position;
			hitNormal = -hit.normal;
		}
	}

	entity->TakeDamage({
		.damage = skillAttack
			? (dynamic_cast<EnemySmall*>(otherActor) ? entity->GetLife() : 300.0f)
			: footAtk ? Random::Range(45.0f, 55.0f) : Random::Range(30.0f, 40.0f),
		.knockBackPower = footAtk ? 5.0f : 0.0f,
		.ignoreDamageCooldown = skillAttack,
		.hitColliderSelf = self,
		.hitColliderOther = other,
		.hitPosition = hitPosition,
		.hitNormal = hitNormal,
		});
	if (skillAttack)
	{
		// SPのヒットストップもグローバル時間倍率で行う。
		// プレイヤーだけでなく敵AI・アニメーション・物理も停止する。
		TimeScaleController::Request(0.35f, 0.0f);
		PostProcessController::Instance().RequestSkillHit();
	}

	if (!entity->IsDead()) lockOnComponent->LockOn(otherActor);
}

// プレイヤーの移動処理
void Player::UpdateMovement()
{
	if (!controller)
	{
		sprinting = false;
		return;
	}
	InputContext ctx = controller->Poll();

	// ---- 入力ベクトルをカメラYaw基準のワールド方向に変換 ----
	float inputLen = sqrtf(ctx.moveX * ctx.moveX + ctx.moveZ * ctx.moveZ);
	const std::string currentStateName = anim ? anim->GetCurrentStateName(0) : "";
	const std::string nextStateName = anim ? anim->GetNextStateName(0) : "";
	const bool isFreeze = (currentStateName.find("Freeze") != std::string::npos); // 動けない
	const bool dodgeAnimationActive =
		currentStateName.find("Quickshift") != std::string::npos ||
		nextStateName.find("Quickshift") != std::string::npos;
	const bool justDodgeSkillAnimationActive =
		currentStateName.starts_with("SpSkill") ||
		nextStateName.starts_with("SpSkill");
	const bool quickStepActive =
		ctx.quickForwardPressed ||
		ctx.quickBackwardPressed ||
		ctx.quickLeftPressed ||
		ctx.quickRightPressed;
	const bool quickStepStarted =
		ctx.quickForwardStarted ||
		ctx.quickBackwardStarted ||
		ctx.quickLeftStarted ||
		ctx.quickRightStarted ||
		ctx.quickDefaultForwardStarted;

	// ジャスト回避から派生したスキル中は、残像と同じく無敵もアニメーション終了まで延長する
	if (justDodgeSkillActive &&
		!justDodgeSkillAnimationActive &&
		!dodgeAnimationActive)
	{
		justDodgeSkillActive = false;
	}
	dodgeInvincible = dodgeAnimationActive || justDodgeSkillActive;
	if (!dodgeInvincible) justDodgeTriggered = false;
	if (cc)
	{
		cc->SetLayerIgnored(Layers::Get("Enemy"), dodgeInvincible);
		cc->SetActorTagIgnored("Enemy", dodgeInvincible);
	}

	const float camYaw =
		cameraController
		? cameraController->GetCameraYaw()
		: 0.0f;
	const float sinY = sinf(camYaw);
	const float cosY = cosf(camYaw);

	const float quickInputX =
		(ctx.quickRightStarted ? 1.0f : 0.0f) -
		(ctx.quickLeftStarted ? 1.0f : 0.0f);
	const float quickInputZ =
		(ctx.quickForwardStarted ? 1.0f : 0.0f) -
		(ctx.quickBackwardStarted ? 1.0f : 0.0f);
	if (ctx.quickDefaultForwardStarted)
	{
		bufferedQuickStepTrigger = "QF";
	}
	else if (fabsf(quickInputX) > eps ||
		fabsf(quickInputZ) > eps)
	{
		Vector3 quickWorldDirection(
			quickInputX * cosY + quickInputZ * sinY,
			0.0f,
			quickInputX * -sinY + quickInputZ * cosY);
		quickWorldDirection.Normalize();

		const float localRight =
			quickWorldDirection.Dot(transform.right);
		const float localForward =
			quickWorldDirection.Dot(transform.forward);
		if (fabsf(localForward) >= fabsf(localRight))
		{
			bufferedQuickStepTrigger =
				localForward >= 0.0f ? "QF" : "QB";
		}
		else
		{
			bufferedQuickStepTrigger =
				localRight >= 0.0f ? "QR" : "QL";
		}
	}

	Vector3 worldMoveDir = Vector3::Zero;
	if (inputLen > 0.1f)
	{
		// 入力(moveX=右, moveZ=前) をカメラ基準でワールドXZ に変換
		worldMoveDir.x = ctx.moveX * cosY + ctx.moveZ * sinY;
		worldMoveDir.z = ctx.moveX * (-sinY) + ctx.moveZ * cosY;
		worldMoveDir.Normalize();
		lockOnComponent->ReleaseIfMovingAway(worldMoveDir);

		// 攻撃中は入力による方向転換を止める
		if (!isFreeze &&
			!dodgeAnimationActive)
		{
			const bool sprintTurn = ctx.sprint && inputLen > 0.1f;
			float turnSpeed = sprintTurn ? 8.0f : 12.0f;
			float targetYaw = atan2f(worldMoveDir.x, worldMoveDir.z);
			Quaternion targetRot = Quaternion::CreateFromYawPitchRoll(targetYaw, 0.0f, 0.0f);
			float t = 1.0f - expf(-turnSpeed * Game::Time::deltaTime);
			transform.SetRotation(Quaternion::Slerp(transform.rotation, targetRot, t));
		}
	}

	// Speed / Sprint パラメータをAnimatorへ
	sprinting =
		!dodgeAnimationActive &&
		ctx.sprint &&
		inputLen > 0.1f;
	float speedParam =
		dodgeAnimationActive || inputLen < 0.1f
		? 0.0f
		: sprinting ? 1.5f : inputLen;
	anim->SetFloat("Speed", speedParam);
	anim->SetBool("IsSprinting", sprinting);
	anim->SetBool("IsDead", IsDead());
	anim->SetBool("IsJustDodge", false);

	const bool startJustDodgeSkill =
		ctx.attackPressed &&
		justDodgeTriggered &&
		dodgeAnimationActive &&
		!justDodgeSkillActive &&
		!IsDead();
	if (startJustDodgeSkill)
	{
		const int skillAnimationIndex = model
			? model->GetAnimationIndex("SS_Sp_Skill1")
			: -1;
		if (skillAnimationIndex >= 0)
		{
			justDodgeTriggered = false;
			justDodgeSkillActive = true;
			justDodgeSkillHitActors.clear();
			lockOnComponent->LockOnNearestEnemy();
			lockOnComponent->SetAimActive(true);
			if (Actor* target = lockOnComponent->GetTarget())
			{
				Vector3 direction = target->transform.position - transform.position;
				direction.y = 0.0f;
				if (direction.LengthSquared() > eps)
				{
					const float targetYaw = atan2f(direction.x, direction.z);
					transform.SetRotation(
						Quaternion::CreateFromYawPitchRoll(targetYaw, 0.0f, 0.0f));
				}
			}

			const float skillDuration =
				model->GetAnimations()[skillAnimationIndex].secondsLength;
			// 必殺技へ入ったら残像は終了するが、時間はゲーム全体を遅くする。
			if (afterimage) afterimage->Clear();
			constexpr float skillTimeScale = 0.45f;
			// アニメーション自体もスケール時間で進むため、
			// 実時間の継続時間を倍率で補正してSP終了まで保つ。
			TimeScaleController::Request(
				skillDuration / skillTimeScale + 0.2f, skillTimeScale);
			anim->SetFloat("Speed", 0.0f);
			anim->SetBool("IsSprinting", false);
			anim->SetBool("IsJustDodge", true);
			sprinting = false;
			if (cameraController)
				cameraController->RequestSkillFocus(skillDuration);
			PostProcessController::Instance().RequestSkillStart();

			// 同じAttack入力を、ジャスト回避中だけSp Skill側へ優先遷移させる
			anim->SetTrigger("Attack");
		}
	}
	else if (ctx.attackPressed && !dodgeAnimationActive && !justDodgeSkillActive)
	{
		lockOnComponent->LockOnNearestEnemy();
		lockOnComponent->SetAimActive(true);
		anim->SetTrigger("Attack");
	}
	const bool canStartDodge =
		dodgeCooldownTimer <= 0.0f &&
		!dodgeAnimationActive &&
		!isFreeze &&
		!IsDead();
	if (quickStepStarted &&
		!bufferedQuickStepTrigger.empty() &&
		canStartDodge)
	{
		dodgeCooldownTimer = dodgeCooldownDuration;
		dodgeInvincible = true;
		justDodgeTriggered = false;
		justDodgeSkillActive = false;
		if (cc) cc->SetLayerIgnored(Layers::Get("Enemy"), true);
		if (cc) cc->SetActorTagIgnored("Enemy", true);
		if (HasIncomingEnemyAttack()) TriggerJustDodge();
		anim->SetFloat("Speed", 0.0f);
		anim->SetBool("IsSprinting", false);
		sprinting = false;
		anim->SetTrigger(
			bufferedQuickStepTrigger);
		bufferedQuickStepTrigger.clear();
	}
	else if (quickStepStarted || !quickStepActive)
	{
		bufferedQuickStepTrigger.clear();
	}

}
