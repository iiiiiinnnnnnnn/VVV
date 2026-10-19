#include "Gameplay/Player/Player.h"
#include "Gameplay/Player/PlayerInput.h"
#include "Application/Input/Input.h"
#include "Gameplay/Camera/ThirdPersonCameraController.h"
#include "Gameplay/Scene/SceneManager.h"
#include "Gameplay/Scene/TestPlayScene.h"
#include "Gameplay/Stage/Component/Terrain.h"
#include "Gameplay/Stage/Component/StageLoader.h"
#include "Application/Time/GameTime.h"
#include "Rendering/Core/Graphics.h"
#include "Gameplay/Scene/PostProcessController.h"
#include "Gameplay/Scene/CameraEffectController.h"
#include "Gameplay/Scene/TimeScaleController.h"
#include "Gameplay/Actor/ActorManager.h"
#include "Gameplay/Actor/Deer.h"
#include "Gameplay/Component/CharacterMotorComponent.h"
#include "Gameplay/Component/LockOnComponent.h"
#include "Physics/Core/PhysicsManager.h"
#include "Physics/Collider/VMDLColliderComponent.h"
#include "Rendering/Component/VMDLModelComponent.h"
#include "Rendering/Component/AfterimageComponent.h"
#include "Rendering/Effect/Effect.h"
#include "Rendering/Effect/EffectManager.h"
#include "Audio/SoundSystem.h"
#include <sol/sol.hpp>
#include <cassert>

void RemapQuickshiftWindTracks(VMDLModel& model)
{
	auto& particleData = model.GetVmdlParticleData();
	const auto findEmitter = [&particleData](const char* name) {
		for (int i = 0; i < static_cast<int>(particleData.emitters.size()); ++i)
			if (::_stricmp(particleData.emitters[i].name.c_str(), name) == 0) return i;
		return -1;
	};
	const int front = findEmitter("WIND_F");
	const int back = findEmitter("WIND_B");
	const int left = findEmitter("WIND_L");
	const int right = findEmitter("WIND_R");
	if (front < 0 || back < 0 || left < 0 || right < 0) return;

	for (auto& track : particleData.tracks)
	{
		if (track.animationName == "SS_Quickshift_F") track.emitterIndex = back;
		else if (track.animationName == "SS_Quickshift_B") track.emitterIndex = front;
		else if (track.animationName == "SS_Quickshift_L") track.emitterIndex = right;
		else if (track.animationName == "SS_Quickshift_R") track.emitterIndex = left;
	}
}

// 生成と初期設定

Player::Player() : Entity("Player", "Player", true, Transform(), 100.0f, 100.0f)
{
	InitializeModel();
	InitializeMovement();
	RegisterAnimationCallbacks();

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

	attackHitEffect = EffectManager::Instance().LoadEffect("critical");
	attackHitEffect->SetBillboard(true);
}

Player::~Player() = default;

void Player::InitializeModel()
{
	// VMDL読み込み
	vmdl = AddComponent<VMDL>("Player/CombatGirls_Sword_Shield");
	model = vmdl->GetSharedModel();
	vmdl->SetAutoUpdateTransform(false);
	vmdl->SetModelYawOffset(RAD(180.0f));
	RemapQuickshiftWindTracks(*model);

	// 足音切り替え用
	footSound = vmdl->GetSoundSource("FOOTSTEP");

	// 状態遷移とゲーム固有コールバックはAnimator側で設定する
	anim = AddComponent<Animator>(model);
	vmdl->BuildFootIK(anim);
	anim->Load("Player.animator");
}

void Player::InitializeMovement()
{
	// キャラクターコントローラ生成
	constexpr float radius = 0.25f;
	constexpr float totalHeight = 1.7f;
	constexpr float capsuleHeight = totalHeight - radius * 2.0f;

	cc = AddComponent<CharacterController>(Layers::Get("Player"), radius, capsuleHeight);
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
}

void Player::RegisterAnimationCallbacks()
{
	// 遷移先の開始時に両方のフラグを更新し、攻撃中断や連続攻撃にも対応する。
	anim->AddCallbackFunc("LockOnAttack", [this](const Animator::State&) {
		lockOnComponent->SetAimActive(true);
		lockOnComponent->SetRotationPaused(justDodgeSkillActive);
	}, {});
	anim->AddCallbackFunc("LockOnHit", [this](const Animator::State&) {
		lockOnComponent->SetAimActive(false);
		lockOnComponent->SetRotationPaused(true);
	}, {});
	anim->AddCallbackFunc("LockOnIdle", [this](const Animator::State&) {
		lockOnComponent->SetAimActive(false);
		lockOnComponent->SetRotationPaused(justDodgeSkillActive);
	}, {});
	anim->AddCallbackFunc("DodgeInvincible", [this](const Animator::State&) {
		justDodgeWindowActive = true;
		dodgeInvincible = true;
	}, [this](const Animator::State&) {
		justDodgeWindowActive = false;
		dodgeInvincible = justDodgeSkillActive;
	});
	anim->AddCallbackFunc("JustDodgeSkillStart", [this](const Animator::State&) {
		ActivateJustDodgeSkill();
	}, {});
	anim->BindCallbacks();
}

void Player::DeathOfFall()
{
	// プレイヤーは落下死せずに上にTP
	transform.SetPosition(
		transform.position.x, 
		transform.position.y + (deathOfFallHeight * -1.0f), 
		transform.position.z);
}

void Player::SetSpawnTransform(const Transform& spawnTransform)
{
	transform.position = spawnTransform.position;
	transform.rotation = spawnTransform.rotation;
	transform.Update();
	if (cc) cc->SetFootPosition(spawnTransform.position);
}

// 毎フレームの更新

void Player::OnUpdate()
{
	Entity::OnUpdate();
	UpdateAttackContacts();
	UpdateWaterDamage();
	UpdateFootSound();
	UpdateMovement();
	UpdateBattleStatus();
	UpdateHealth();
	if (dodgeInvincible)
		PostProcessController::Instance().RequestInvincibilityAura();
	if (motor)
	{
		motor->SetExternalVelocity(knockBackVelocity);
	}

	UpdateTerrainDeformInput();
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
	ImGui::Text((const char*)u8"回避無敵時間: %s", dodgeInvincible ? "Yes" : "No");
	ImGui::Text((const char*)u8"ジャスト回避: %s", justDodgeTriggered ? "Yes" : "No");
	ImGui::Text((const char*)u8"回避待機時間: %.2f", dodgeCooldownTimer);
}

void Player::UpdateAttackContacts()
{
	for (auto& [attack, targets] : attackContacts)
	{
		for (auto& [actor, contact] : targets)
			std::erase_if(contact.colliders, [](PhysicsComponent* collider) {
				return !PhysicsComponent::IsLive(collider) || !collider->IsActive();
			});
		std::erase_if(targets, [](const auto& target) {
			return target.second.target.expired() || target.second.colliders.empty();
		});
	}
	std::erase_if(attackContacts, [](const auto& attack) {
		return !PhysicsComponent::IsLive(attack.first) || !attack.first->IsActive() || attack.second.empty();
	});
}

void Player::UpdateTerrainDeformInput()
{
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

void Player::UpdateFootSound()
{
	if (!footSound) return;

	Scene* scene = SceneManager::Instance().GetCurrentScene();
	Stage* stage = scene ? scene->GetCurrentStage() : nullptr;
	Terrain* terrain = stage ? stage->GetComponent<Terrain>() : nullptr;
	if (!terrain) return;

	const std::string layerName = terrain->GetSurfaceLayerName(transform.position);
	if (isInWater)
	{
		footSound->soundPath = "SE/Player/pl_water";
	}
	else if (layerName == "grass")
	{
		footSound->soundPath = "SE/Player/pl_walk_grass";
	}
	else if (layerName == "rock" || layerName == "stone" || layerName == "dirt")
	{
		footSound->soundPath = "SE/Player/pl_walk_rock";
	}
	else
	{
		footSound->soundPath = "";
	}
}

void Player::UpdateBattleStatus()
{
	inBattleTimer -= Game::Time::deltaTime;
}

void Player::UpdateWaterDamage()
{
	// Sceneから水位持ってくるのめっちゃめんどいですが、とりあえずこのままでいきます。
	// 何か問題あれば直す
	Scene* scene = SceneManager::Instance().GetCurrentScene();
	Stage* stage = scene ? scene->GetCurrentStage() : nullptr;
	StageLoader* stageLoader = stage ? stage->GetComponent<StageLoader>() : nullptr;
	// 水ある＝水-0.4でダメージ、ない＝-0.3でダメージ
	float waterDamageHeight = stageLoader ? stageLoader->GetWorldWaterHeight() - 0.4f : -0.3f;
	isInWater = transform.position.y < waterDamageHeight;

	// vignette : 毒沼
	PostProcessController::Instance().SetPoisonVignette(!deathSequenceActive && isInWater);

	if (deathSequenceActive)
	{
		UpdateDeathSequence();
		if (motor) motor->SetExternalVelocity(knockBackVelocity);
		return;
	}
	const float remainingCooldown = dodgeCooldownTimer - Game::Time::unscaledDeltaTime;
	dodgeCooldownTimer = std::max(remainingCooldown, 0.0f);

	// damageHeightCoolDownでダメージ食らわせる
	if (isInWater)
	{
		if (damageLowerHeightTimer > damageLowerHeightCoolDown)
		{
			DamageData dmgData;
			dmgData.damage = damageWaterAmount;
			dmgData.hitPosition.reset();
			dmgData.targetCollider = nullptr;

			TakeDamage(dmgData);

			vmdl->GetRenderer()->PlaySoundSource("WATER_DAMAGE");

			damageLowerHeightTimer = 0;
		}
		damageLowerHeightTimer += Game::Time::deltaTime; // 増
	}
	else
	{
		damageLowerHeightTimer = 0.0f;
	}
}

void Player::UpdateMovement()
{
	actionInputThisFrame = false;
	auto* playerInput = GetPlayerInput();
	if (!playerInput)
	{
		dodgeInputBufferTimer = 0.0f;
		sprinting = false;
		return;
	}

	const InputContext input = playerInput->IsActive() ? playerInput->Poll() : InputContext{};
	const ActionState state = GetActionState();
	const float inputLengthSquared = input.moveX * input.moveX + input.moveZ * input.moveZ;
	const float inputLength = sqrtf(inputLengthSquared);
	const bool quickStepHeld = input.Held(PlayerAction::QuickForward) || input.Held(PlayerAction::QuickBackward) ||
		input.Held(PlayerAction::QuickLeft) || input.Held(PlayerAction::QuickRight);
	const bool quickStepStarted = input.Down(PlayerAction::QuickForward) || input.Down(PlayerAction::QuickBackward) ||
		input.Down(PlayerAction::QuickLeft) || input.Down(PlayerAction::QuickRight) || input.Down(PlayerAction::QuickDefaultForward);

	if (input.Down(PlayerAction::AlignCamera) && cameraController)
		cameraController->RequestAlignToPlayerForward();

	const bool actionBlocksCrouch = state.frozen || IsDead() || state.attacking ||
		state.dodging || state.skill || state.hit;
	const bool inputBlocksCrouch = input.Down(PlayerAction::Attack) || quickStepStarted;
	crouching = input.Held(PlayerAction::Crouch) && !actionBlocksCrouch && !inputBlocksCrouch;

	actionInputThisFrame = inputLength > 0.1f || input.Held(PlayerAction::Crouch) || input.Held(PlayerAction::Sprint) ||
		input.Down(PlayerAction::Attack) || quickStepHeld || quickStepStarted;

	UpdateDodgeState(state);
	// 移動による旋回の前に、カメラ基準の回避方向を確定する。
	Vector3 quickShiftDirection(0.0f, 0.0f, 0.0f);
	if (input.Down(PlayerAction::QuickDefaultForward))
		quickShiftDirection = transform.forward;
	else if (quickStepStarted)
	{
		const float directionX = (input.Down(PlayerAction::QuickRight) ? 1.0f : 0.0f) -
			(input.Down(PlayerAction::QuickLeft) ? 1.0f : 0.0f);
		const float directionZ = (input.Down(PlayerAction::QuickForward) ? 1.0f : 0.0f) -
			(input.Down(PlayerAction::QuickBackward) ? 1.0f : 0.0f);
		if (fabsf(directionX) > eps || fabsf(directionZ) > eps)
			quickShiftDirection = GetCameraRelativeDirection(directionX, directionZ);
	}
	// 回避終了直前の押下を短時間保持し、操作可能になった最初のフレームで使う。
	dodgeInputBufferTimer = std::max(dodgeInputBufferTimer - Game::Time::unscaledDeltaTime, 0.0f);
	if (!playerInput->IsActive() || IsDead() || state.frozen || justDodgeSkillActive)
		dodgeInputBufferTimer = 0.0f;
	else if (quickStepStarted && quickShiftDirection.LengthSquared() > eps * eps)
	{
		bufferedDodgeDirection = quickShiftDirection;
		dodgeInputBufferTimer = dodgeInputBufferDuration;
	}
	UpdateMoveDirection(input, inputLength, state);
	UpdateMovementAnimation(input, inputLength, state);
	HandleAttackInput(input, state);
	if (dodgeInputBufferTimer > 0.0f) QuickShift(bufferedDodgeDirection);
}

Player::ActionState Player::GetActionState() const
{
	const std::string current = anim ? anim->GetCurrentStateName() : "";
	const std::string next = anim ? anim->GetNextStateName() : "";

	ActionState state;
	state.frozen = current.find("Freeze") != std::string::npos;
	state.dodging = current.find("Quickshift") != std::string::npos ||
		next.find("Quickshift") != std::string::npos;
	state.skill = current.starts_with("SpSkill") || next.starts_with("SpSkill");
	state.attacking = current.starts_with("Attack") || next.starts_with("Attack");
	state.hit = current.starts_with("Hit_") || next.starts_with("Hit_");
	return state;
}

void Player::UpdateDodgeState(const ActionState& state)
{
	// 歩行へ戻った後まで、固定クールタイムだけが残らないようにする。
	if (dodgeWasActive && !state.dodging) dodgeCooldownTimer = 0.0f;
	dodgeWasActive = state.dodging;

	// ジャスト回避から派生したスキル中は、残像と同じく無敵もアニメーション終了まで延長する
	if (justDodgeSkillActive &&
		!state.skill &&
		!state.dodging)
	{
		justDodgeSkillActive = false;
		justDodgeSkillTarget.reset();
		lockOnComponent->SetRotationPaused(false);
	}
	if (!state.dodging) justDodgeWindowActive = false;
	dodgeInvincible = justDodgeWindowActive || justDodgeSkillActive;
	if (!state.dodging && !justDodgeSkillActive)
	{
		justDodgeTriggered = false;
		justDodgeAttacker = nullptr;
	}
	if (cc)
	{
		cc->SetLayerIgnored(Layers::Get("Enemy"), dodgeInvincible);
		cc->SetActorTagIgnored("Enemy", dodgeInvincible);
	}
}

Vector3 Player::GetCameraRelativeDirection(float inputX, float inputZ) const
{
	const float cameraYaw = cameraController ? cameraController->GetCameraYaw() : 0.0f;
	const float sinYaw = sinf(cameraYaw);
	const float cosYaw = cosf(cameraYaw);
	const float worldX = inputX * cosYaw + inputZ * sinYaw;
	const float worldZ = inputX * -sinYaw + inputZ * cosYaw;

	Vector3 direction(worldX, 0.0f, worldZ);
	direction.Normalize();
	return direction;
}

void Player::UpdateMoveDirection(const InputContext& input, float inputLength, const ActionState& state)
{
	if (inputLength <= 0.1f) return;

	const Vector3 moveDirection = GetCameraRelativeDirection(input.moveX, input.moveZ);
	if (!justDodgeSkillActive) lockOnComponent->ReleaseIfMovingAway(moveDirection);
	if (state.frozen || state.dodging || justDodgeSkillActive) return;

	const float turnSpeed = input.Held(PlayerAction::Sprint) ? 8.0f : 12.0f;
	const float targetYaw = atan2f(moveDirection.x, moveDirection.z);
	const Quaternion targetRotation = Quaternion::CreateFromYawPitchRoll(targetYaw, 0.0f, 0.0f);

	const float deltaTime = Game::Time::deltaTime;
	const float remainingAngle = expf(-turnSpeed * deltaTime);
	const float turnAmount = 1.0f - remainingAngle;
	const Quaternion nextRotation = Quaternion::Slerp(transform.rotation, targetRotation, turnAmount);
	transform.SetRotation(nextRotation);
}

void Player::UpdateMovementAnimation(const InputContext& input, float inputLength, const ActionState& state)
{
	// Speed / Sprint パラメータをAnimatorへ
	sprinting =
		!crouching &&
		!state.dodging && !justDodgeSkillActive &&
		input.Held(PlayerAction::Sprint) &&
		inputLength > 0.1f;
	float speedParam = 0.0f;
	if (!state.dodging && !justDodgeSkillActive && inputLength >= 0.1f)
		speedParam = sprinting ? 1.5f : inputLength;

	anim->SetFloat("Speed", speedParam);
	anim->SetBool("IsSprinting", sprinting);
	anim->SetBool("IsCrouching", crouching);
	anim->SetBool("IsDead", IsDead());
	anim->SetBool("IsJustDodge", false);
	if (motor)
	{
		// 水に入ってたらゆっくり
		float rootMotionScale = crouching ? crouchRootMotionScale : 1.0f;
		if (isInWater) rootMotionScale *= waterMoveSpeedScale;
		motor->SetRootMotionScale(rootMotionScale);
	}
}

void Player::HandleAttackInput(const InputContext& input, const ActionState& state)
{
	// もしSpSkill2Freezeなら、0.235f~0.345fにクリックすると続きの攻撃(剣で刺すやつ)になります
	const bool skillContinuationWindow = anim->GetCurrentStateName() == "SpSkill2Freeze";
	if (!skillContinuationWindow) anim->SetBool("SkillContinue", false);
	if (!input.Down(PlayerAction::Attack)) return;
	if (skillContinuationWindow)
	{
		const auto& layer = anim->GetLayer(0);
		const float length = anim->GetStateLength(layer.states[layer.currentStateIndex]);
		const float progress = length > 0.0f ? layer.currentTime / length : 0.0f;
		if (!anim->IsTransitioning() && progress >= 0.235f && progress <= 0.345f)
			anim->SetBool("SkillContinue", true);
		return;
	}

	Actor* target = GetJustDodgeTarget();
	const bool canUseSkill = justDodgeTriggered && target && state.dodging &&
		!justDodgeSkillActive && !IsDead();
	if (canUseSkill)
	{
		StartJustDodgeSkill(target);
		return;
	}

	if (state.dodging || justDodgeSkillActive) return;
	lockOnComponent->LockOnNearestEnemy();
	anim->SetTrigger("Attack");
}

Actor* Player::GetJustDodgeTarget() const
{
	if (!justDodgeTriggered) return nullptr;
	const auto* manager = ActorManager::GetActive();
	if (!manager || !manager->Contains(justDodgeAttacker)) return nullptr;
	if (!justDodgeAttacker->IsActive() || justDodgeAttacker->IsPendingDestroy()) return nullptr;

	const auto* entity = dynamic_cast<const Entity*>(justDodgeAttacker);
	if (entity && entity->IsDead()) return nullptr;
	return justDodgeAttacker;
}

void Player::StartJustDodgeSkill(Actor* target)
{
	if (!target || !anim || !model || anim->GetLayerCount() == 0 || IsDead()) return;
	const std::string retreatState = "SpSkill" + std::to_string(anim->GetInt("SkillVariant")) + "QuickshiftB";
	if (!anim->GetLayer(0).GetState(retreatState)) return;

	// 対象へ向ける処理だけを行い、後方回避から必殺技への接続はAnimatorに任せる。
	Vector3 direction = target->transform.position - transform.position;
	direction.y = 0.0f;
	if (direction.LengthSquared() > eps * eps)
	{
		const float targetYaw = atan2f(direction.x, direction.z);
		transform.SetRotation(Quaternion::CreateFromYawPitchRoll(targetYaw, 0.0f, 0.0f));
	}
	justDodgeTriggered = false;
	justDodgeSkillTarget = target->weak_from_this();
	justDodgeSkillActive = true;
	dodgeInputBufferTimer = 0.0f;
	dodgeInvincible = true;
	crouching = false;
	sprinting = false;
	dodgeCooldownTimer = dodgeCooldownDuration;
	lockOnComponent->LockOn(target);
	lockOnComponent->SetRotationPaused(true);
	anim->SetFloat("Speed", 0.0f);
	anim->SetBool("IsSprinting", false);
	anim->SetBool("IsCrouching", false);
	anim->SetBool("IsJustDodge", true);
	anim->SetTrigger("Attack");
}

void Player::ActivateJustDodgeSkill()
{
	if (!justDodgeSkillActive || IsDead()) return;

	// 必殺技ステートの開始コールバック。離脱後の対象の位置へ向き直す。
	auto target = justDodgeSkillTarget.lock();
	const auto* manager = ActorManager::GetActive();
	if (target && manager && manager->Contains(target.get()) && target->IsActive() && !target->IsPendingDestroy())
	{
		Vector3 direction = target->transform.position - transform.position;
		direction.y = 0.0f;
		if (direction.LengthSquared() > eps * eps)
		{
			const float targetYaw = atan2f(direction.x, direction.z);
			transform.SetRotation(Quaternion::CreateFromYawPitchRoll(targetYaw, 0.0f, 0.0f));
		}
	}
	lockOnComponent->SetRotationPaused(true);

	const float skillDuration = 0.5f;
	if (afterimage) afterimage->Play(skillDuration); // ずつきまで
	// SP攻撃の入りだけを遅くして、以降は通常速度に戻す
	constexpr float skillTimeScale = 0.45f;
	constexpr float skillSlowDuration = 0.18f;
	TimeScaleController::Request(skillSlowDuration, skillTimeScale);
	anim->SetFloat("Speed", 0.0f);
	anim->SetBool("IsSprinting", false);
	anim->SetBool("IsJustDodge", true);
	sprinting = false;
	if (cameraController)
		cameraController->RequestSkillFocus(skillDuration);
	PostProcessController::Instance().RequestSkillStart();

}

// 被ダメージとジャスト回避

void Player::TakeDamage(const DamageData& damageData)
{
	Actor* attacker = damageData.attackerCollider
		? dynamic_cast<Actor*>(damageData.attackerCollider->GetOwner())
		: nullptr;
	if (damageData.attackerCollider && damageData.attackerCollider->GetLayerId() == Layers::Get("EnemyAtk") &&
		!IsEnemyAttackActive(attacker, damageData.attackerCollider)) return;
	if (dodgeInvincible) // 回避中
	{
		// 敵の攻撃がAtkかどうか(有効かどうか)
		if (IsEnemyAttackActive(attacker, damageData.attackerCollider))
		{
			// ジャスト回避
			if (justDodgeTriggered || justDodgeSkillActive)
				vmdl->GetRenderer()->PlayParticleEmitter("JUSTDODGE");
			else TriggerJustDodge(attacker);
		}
		return;
	}
	Entity::TakeDamage(damageData);
}

bool Player::IsEnemyAttackActive(const Actor* enemy, const PhysicsComponent* collider) const
{
	if (!enemy || !collider) return false;
	if (!enemy->IsActive() || enemy->IsPendingDestroy()) return false;
	if (!enemy->CompareTag("Enemy")) return false;
	if (collider->GetOwner() != enemy || !collider->IsActive()) return false;
	if (collider->GetLayerId() != Layers::Get("EnemyAtk")) return false;
	const auto* entity = dynamic_cast<const Entity*>(enemy);
	return !entity || !entity->IsDead();
}

void Player::OnDamaged(const DamageData& damageData)
{
	justDodgeSkillTarget.reset();
	healthRecoveryTimer = 0.0f;
	CameraEffectController::Request(0.13f, 0.07f);
	PostProcessController::Instance().RequestDamagedVignette(
		0.7f, 0.6f, 0.17f, Easing::Type::InSine, Easing::Type::OutCubic);

	// 本来はanimatorに任せたほうがいいとは思う
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

		inBattleTimer = BattleTimerDuration;
	}
	else
	{
		if (damageData.targetCollider) anim->SetTrigger("Hit_R");
	}
	lockOnComponent->PauseRotation(0.15f);
}

void Player::TriggerJustDodge(Actor* attacker)
{
	if (justDodgeTriggered || justDodgeSkillActive || !justDodgeWindowActive) return;
	justDodgeAttacker = attacker;
	justDodgeTriggered = true;
	if (afterimage) afterimage->Play(0.5f);

	CameraEffectController::Request(0.13f, 0.045f);
	CameraEffectController::RequestFovOffset(0.24f, 8.0f);
	TimeScaleController::Request(0.20f, 0.03f);
	PostProcessController::Instance().RequestJustDodge();
}

// 攻撃の接触とダメージ

void Player::OnTriggerEnter(
	PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal)
{
	if (!self || !other) return;
	if (IsDead() || !self->IsActive() || !other->IsActive()) return;

	// 体に敵の攻撃が触れたときは、ジャスト回避を判定する。
	const bool bodyContact = self->GetLayerId() == Layers::Get("Player");
	if (bodyContact && dodgeInvincible && !justDodgeSkillActive)
	{
		Actor* attacker = dynamic_cast<Actor*>(other->GetOwner());
		if (IsEnemyAttackActive(attacker, other)) TriggerJustDodge(attacker);
	}

	// 攻撃コライダーの接触は、敵へのダメージ処理へ渡す。
	if (self->GetLayerId() == Layers::Get("PlayerAtk"))
		HandleAttackContact(self, other, point, normal);
}

void Player::HandleAttackContact(
	PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal)
{
	Entity* target = dynamic_cast<Entity*>(other->GetOwner());
	if (!target || target->IsDead()) return;
	if (!target->IsActive() || target->IsPendingDestroy()) return;
	if (!target->CompareTag("Enemy") && !target->CompareTag("Crystal")) return;

	auto* attack = dynamic_cast<VMDLColliderComponent*>(self);
	if (!attack) return;
	if (!RegisterAttackContact(self, other, target)) return;

	Vector3 hitPosition = point;
	Vector3 hitNormal = normal;
	ResolveAttackHit(attack, target, hitPosition, hitNormal);

	const bool skillAttack = justDodgeSkillActive || GetActionState().skill;
	const bool kick = self->CompareName("kick");
	const float damage = GetAttackDamage(target, skillAttack, kick);
	const float knockBackPower = kick ? 5.0f : 0.0f;
	target->TakeDamage({
		.damage = damage,
		.knockBackPower = knockBackPower,
		.isForceDamage = skillAttack,
		.specialAttack = skillAttack,
		.attackerCollider = self,
		.targetCollider = other,
		.hitPosition = hitPosition,
		.hitNormal = hitNormal,
	});

	inBattleTimer = BattleTimerDuration;

	PlayAttackHitEffect(hitPosition);
	if (skillAttack)
	{
		TimeScaleController::Request(0.35f, 0.0f);
		PostProcessController::Instance().RequestSkillHit();
	}
	else if (!target->IsDead())
	{
		lockOnComponent->LockOn(target);
	}
}

bool Player::RegisterAttackContact(PhysicsComponent* attack, PhysicsComponent* other, Actor* target)
{
	// 同じ攻撃で同じ敵に触れている間は一回。別の攻撃や再接触は受け付ける。
	AttackContact& contact = attackContacts[attack][target];
	const bool alreadyTouching = !contact.target.expired() && !contact.colliders.empty();
	if (contact.target.expired()) contact.colliders.clear();
	contact.target = target->weak_from_this();
	contact.colliders.insert(other);
	return !alreadyTouching;
}

void Player::ResolveAttackHit(
	VMDLColliderComponent* attack, Actor* target, Vector3& hitPosition, Vector3& hitNormal)
{
	const Vector3 rayOrigin = attack->GetWorldPosition();
	Vector3 rayDirection = hitPosition - rayOrigin;
	if (rayDirection.LengthSquared() <= eps)
		rayDirection = target->transform.position - rayOrigin;
	if (rayDirection.LengthSquared() <= eps) return;

	const float contactDistance = rayDirection.Length();
	const float rayDistance = contactDistance * 2.0f + 2.0f;
	rayDirection.Normalize();

	PhysicsManager::PhysicsRaycastHit hit;
	const bool foundHit = PhysicsManager::Instance().Raycast(
		rayOrigin, rayDirection, rayDistance, hit, attack->GetLayerId(), this);
	if (!foundHit) return;

	hitPosition = hit.position;
	hitNormal = -hit.normal;
}

float Player::GetAttackDamage(const Entity* target, bool skillAttack, bool kick) const
{
	if (skillAttack)
	{
		if (dynamic_cast<const Deer*>(target)) return target->GetLife();
		return 200.0f;
	}

	if (kick) return Random::Range(45.0f, 55.0f);
	return Random::Range(30.0f, 40.0f);
}

void Player::OnTriggerExit(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal)
{
	auto attack = attackContacts.find(self);
	if (attack == attackContacts.end()) return;
	for (auto& [actor, contact] : attack->second) contact.colliders.erase(other);
	std::erase_if(attack->second, [](const auto& target) {
		return target.second.colliders.empty();
	});
	if (attack->second.empty()) attackContacts.erase(attack);
}

void Player::PlayAttackHitEffect(const Vector3& position)
{
	if (!attackHitEffect || !attackHitEffect->IsValid()) return;
	attackHitEffect->Play(position, 0.15f);
}

void Player::QuickShift(const Vector3& direction, bool force)
{
	if (!anim || IsDead()) return;
	if (!force)
	{
		if (dodgeCooldownTimer > 0.0f || justDodgeSkillActive) return;
		const ActionState state = GetActionState();
		if (state.frozen || state.dodging) return;
	}

	Vector3 dodgeDirection = direction;
	dodgeDirection.y = 0.0f;
	if (dodgeDirection.LengthSquared() <= eps * eps) return;
	dodgeDirection.Normalize();

	dodgeInputBufferTimer = 0.0f;

	// 向きを判断
	const float localRight = dodgeDirection.Dot(transform.right);
	const float localForward = dodgeDirection.Dot(transform.forward);
	const char* trigger;
	if (fabsf(localForward) >= fabsf(localRight)) trigger = localForward >= 0.0f ? "QF" : "QB";
	else trigger = localRight >= 0.0f ? "QR" : "QL";

	dodgeCooldownTimer = dodgeCooldownDuration;
	dodgeInvincible = false;
	justDodgeWindowActive = false;
	justDodgeTriggered = false;
	justDodgeAttacker = nullptr;
	justDodgeSkillActive = false;
	crouching = false;
	sprinting = false;
	actionInputThisFrame = true;

	anim->SetFloat("Speed", 0.0f);
	anim->SetBool("IsSprinting", false);
	anim->SetBool("IsCrouching", false);
	anim->SetTrigger(trigger);
}

// 回復と死亡演出

void Player::UpdateHealth()
{
	if (!anim || IsDead() || life >= maxLife || IsInBattle())
	{
		healthRecoveryTimer = 0.0f;
		return;
	}

	const std::string& currentState = anim->GetCurrentStateName();
	const std::string& nextState = anim->GetNextStateName();
	const bool currentIdle = currentState == "Idle" || currentState == "CrouchIdle";
	const bool nextIdle = nextState == "Idle" || nextState == "CrouchIdle";
	const bool stayingIdle = !anim->IsTransitioning() || nextState.empty() || nextIdle;
	const bool actionActive = actionInputThisFrame || dodgeInvincible || justDodgeSkillActive;
	const bool idle = currentIdle && stayingIdle && !actionActive;
	if (!idle)
	{
		healthRecoveryTimer = 0.0f;
		return;
	}

	healthRecoveryTimer += Game::Time::deltaTime;
	while (healthRecoveryTimer >= healthRecoveryInterval && life < maxLife)
	{
		healthRecoveryTimer -= healthRecoveryInterval;
		Heal(healthRecoveryAmount);
	}
}

void Player::OnDead(const DamageData& damageData)
{
	dodgeInputBufferTimer = 0.0f;
	justDodgeSkillTarget.reset();
	justDodgeWindowActive = false;
	justDodgeAttacker = nullptr;
	deathSequenceActive = true;
	deathReloadRequested = false;
	deathSequenceTimer = 0.0f;
	healthRecoveryTimer = 0.0f;
	sprinting = false;
	if (anim) anim->SetBool("IsDead", true);
	lockOnComponent->ClearTarget();
	lockOnComponent->SetActive(false);
}

void Player::OnHeal(float amount)
{
	// 回復エフェクト
	vmdl->GetRenderer()->PlayParticleEmitter("HEAL");
}

void Player::UpdateDeathSequence()
{
	deathSequenceTimer += Game::Time::unscaledDeltaTime;
	const float progress = std::clamp(
		deathSequenceTimer / deathVignetteDuration, 0.0f, 1.0f);
	constexpr float maximumVignetteProgress = 0.3f;
	PostProcessController::Instance().RequestDeathVignette(
		Easing::Evaluate(progress, Easing::Type::InSine) * maximumVignetteProgress);

	if (progress >= 1.0f && !deathReloadRequested)
	{
		deathReloadRequested =
			SceneManager::Instance().LoadScene<TestPlayScene>();
	}
}
