// Player.cpp

#include "Gameplay/Player/Player.h"
#include "Gameplay/Camera/ThirdPersonCameraController.h"
#include "Application/Time/GameTime.h"
#include "Rendering/Core/Graphics.h"
#include "Gameplay/Scene/PostProcessController.h"
#include "Gameplay/Scene/CameraEffectController.h"
#include "Gameplay/Actor/ActorManager.h"
#include "Gameplay/Component/CharacterMotorComponent.h"
#include "Gameplay/Component/LockOnComponent.h"
#include "Physics/Core/PhysicsManager.h"
#include "Physics/Collider/VMDLColliderComponent.h"
#include "Rendering/Component/VMDLModelComponent.h"

Player::Player() : Entity("Player", "Player", true, 100.0f, 100.0f)
{
	// VMDL読み込み
	vmdl = AddComponent<VMDL>("Data/Model/CombatGirl_Shield/CombatGirls_Sword_Shield");
	model = vmdl->GetSharedModel();
	vmdl->SetAutoUpdateTransform(false);

	// 状態遷移とゲーム固有コールバックはAnimator側で設定する。
	anim = vmdl->GetAnimator();
	anim->Load("Data/Animator/Player.animator");
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
	lockOnComponent->SetLostRange(20.0f);
	lockOnComponent->SetRotationSpeed(6.0f);

	// SetFootPositionとSetPositionは両方呼ばない
	cc->SetFootPosition({ 21.459f, 0, 7.514f });

	// LookAt
	auto lookAt = AddComponent<LookAt>(
		model.get(), "head", "neck_01");
	lookAt->SetLookDistance(10.0f);
	lookAt->SetFilterTags({"Enemy"});

	// プレイヤーはダメージのクールダウン長い
	GetCooldowns().DamageCooldownDuration = 1.5f;
}

void Player::OnUpdate()
{
	Entity::OnUpdate();

	const std::string& stateName = anim->GetCurrentStateName();
	const bool isAttacking = stateName.starts_with("Attack");
	const bool isHit = stateName == "Hit_RFreeze" || stateName == "Hit_LFreeze";
	lockOnComponent->SetAimActive(isAttacking);
	lockOnComponent->SetRotationPaused(isHit || IsDead());

	UpdateMovement();
	if (motor)
	{
		motor->SetExternalVelocity(knockBackVelocity);
	}
}

void Player::OnLateUpdate()
{
	model->UpdateTransform(transform.matrix);

	if (cc)
	{
		cc->ClearDebugRenderPosition();
	}
}

void Player::OnDrawGUI()
{
	Entity::OnDrawGUI();
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
		.damage = footAtk ? Random::Range(45.0f, 55.0f) : Random::Range(30.0f, 40.0f),
		.knockBackPower = footAtk ? 5.0f : 0.0f,
		.hitColliderSelf = self,
		.hitColliderOther = other,
		.hitPosition = hitPosition,
		.hitNormal = hitNormal,
		});

	if (!entity->IsDead()) lockOnComponent->LockOn(otherActor);
}

// プレイヤーの移動処理
void Player::UpdateMovement()
{
	if (!controller) return;
	InputContext ctx = controller->Poll();

	// ---- 入力ベクトルをカメラYaw基準のワールド方向に変換 ----
	float inputLen = sqrtf(ctx.moveX * ctx.moveX + ctx.moveZ * ctx.moveZ);
	const std::string currentStateName = anim ? anim->GetCurrentStateName(0) : "";
	const bool isFreeze = (currentStateName.find("Freeze") != std::string::npos); // 動けない
	const bool isQuickshift =
		currentStateName.find("Quickshift") !=
		std::string::npos;
	const bool quickStepActive =
		ctx.quickForwardPressed ||
		ctx.quickBackwardPressed ||
		ctx.quickLeftPressed ||
		ctx.quickRightPressed;
	const bool quickStepStarted =
		ctx.quickForwardStarted ||
		ctx.quickBackwardStarted ||
		ctx.quickLeftStarted ||
		ctx.quickRightStarted;

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
	if (fabsf(quickInputX) > eps ||
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

		// 攻撃中は入力による方向転換を止める
		if (!isFreeze &&
			!isQuickshift &&
			!quickStepActive)
		{
			bool sprinting = ctx.sprint && inputLen > 0.1f;
			float turnSpeed = sprinting ? 8.0f : 12.0f;
			float targetYaw = atan2f(worldMoveDir.x, worldMoveDir.z);
			Quaternion targetRot = Quaternion::CreateFromYawPitchRoll(targetYaw, 0.0f, 0.0f);
			float t = 1.0f - expf(-turnSpeed * Game::Time::deltaTime);
			transform.SetRotation(Quaternion::Slerp(transform.rotation, targetRot, t));
		}
	}

	// Speed / Sprint パラメータをAnimatorへ
	bool sprinting =
		!quickStepActive &&
		ctx.sprint &&
		inputLen > 0.1f;
	float speedParam =
		quickStepActive || inputLen < 0.1f
		? 0.0f
		: sprinting ? 1.5f : inputLen;
	anim->SetFloat("Speed", speedParam);
	anim->SetBool("IsSprinting", sprinting);
	anim->SetBool("IsDead", IsDead());

	if (ctx.attackPressed)
	{
		lockOnComponent->SetAimActive(true);
		anim->SetTrigger("Attack");
	}
	if (quickStepStarted &&
		!bufferedQuickStepTrigger.empty())
	{
		anim->SetTrigger(
			bufferedQuickStepTrigger);
		bufferedQuickStepTrigger.clear();
	}
	else if (!quickStepActive)
	{
		bufferedQuickStepTrigger.clear();
	}

}
