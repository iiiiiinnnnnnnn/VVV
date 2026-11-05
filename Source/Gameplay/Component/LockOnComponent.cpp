#include "Application/SettingsAndDebug/ProfileScope.h"
#include "Gameplay/Component/LockOnComponent.h"

#include "Application/Time/GameTime.h"
#include "Gameplay/Actor/Actor.h"
#include "Gameplay/Actor/ActorManager.h"
#include "Gameplay/Actor/Entity.h"

LockOnComponent::LockOnComponent(Object* owner)
	: Component(owner)
{
}

void LockOnComponent::OnAwake()
{
	ownerEntity = dynamic_cast<Entity*>(owner);
}

void LockOnComponent::OnUpdate()
{
	PROFILE_FUNCTION();
	rotationPauseTimer.Update(Game::Time::deltaTime);

	if (!ownerEntity || ownerEntity->IsDead())
	{
		ClearTarget();
		return;
	}
	if (target && !IsTargetValid()) ClearTarget();

	Transform* ownerTransform = owner->GetTransform();
	if (!ownerTransform) return;

	Vector3 direction = Vector3::Zero;
	if (target)
	{
		direction = target->transform.position - ownerTransform->position;
		direction.y = 0.0f;
		if (direction.LengthSquared() > lostRange * lostRange) ClearTarget();
	}
	if (!target) return;
	if (!aimActive || ownerEntity->IsDead() || rotationPaused || rotationPauseTimer.GetRemaining() > 0.0f) return;
	if (direction.LengthSquared() <= eps) return;
	direction.Normalize();

	Vector3 forward = ownerTransform->GetForward();
	forward.y = 0.0f;
	if (forward.LengthSquared() <= eps) forward = Vector3::UnitZ;
	forward.Normalize();

	const float targetYaw = atan2f(direction.x, direction.z);
	const Quaternion targetRotation =
		Quaternion::CreateFromYawPitchRoll(targetYaw, 0.0f, 0.0f);
	const float rate =
		1.0f - expf(-rotationSpeed * Game::Time::deltaTime);
	ownerTransform->SetRotation(
		Quaternion::Slerp(ownerTransform->rotation, targetRotation, rate));
}

void LockOnComponent::OnDisabled()
{
	ClearTarget();
}

void LockOnComponent::OnDrawGUI()
{
	if (target && !IsTargetValid()) ClearTarget();
	ImGui::Text("Target: %s", target ? target->GetName().c_str() : "None");
	ImGui::DragFloat("Acquire Range", &acquireRange, 0.1f, 0.0f, 1000.0f);
	ImGui::DragFloat("Lost Range", &lostRange, 0.1f, 0.0f, 1000.0f);
	ImGui::DragFloat("Rotation Speed", &rotationSpeed, 0.1f, 0.0f, 30.0f);
	ImGui::Text("Aim Active: %s", aimActive ? "true" : "false");
	ImGui::Text("Rotation Paused: %s", rotationPaused ? "true" : "false");
	ImGui::Text("Rotation Pause: %.2f", rotationPauseTimer.GetRemaining());
	if (target && ImGui::Button("Clear Target")) ClearTarget();
}

void LockOnComponent::LockOn(Actor* actor)
{
	if (!actor || actor == owner || actor->IsPendingDestroy()) return;
	const Entity* entity = dynamic_cast<const Entity*>(actor);
	if (entity && entity->IsDead())
	{
		if (target == actor) ClearTarget();
		return;
	}
	if (target == actor && IsTargetValid()) return;
	target = actor;
	targetLifetime = actor->weak_from_this();
}

// 攻撃時は既存のロックオン対象を優先し、対象がなければ最寄りの敵を選ぶ。
Actor* LockOnComponent::GetAttackCandidate() const
{
	if (!IsActive() || !ownerEntity || !owner->IsActive() || ownerEntity->IsDead()) return nullptr;
	const Transform* ownerTransform = owner->GetTransform();
	if (!ownerTransform) return nullptr;
	if (IsTargetValid())
	{
		Vector3 difference = target->transform.position - ownerTransform->position;
		difference.y = 0.0f;
		if (difference.LengthSquared() <= lostRange * lostRange) return target;
	}
	ActorManager* actorManager = ActorManager::GetActive();
	if (!actorManager) return nullptr;

	Actor* nearest = nullptr;
	float nearestDistanceSquared = acquireRange * acquireRange;
	for (Actor* actor : actorManager->GetActorsByTag("Enemy"))
	{
		if (!actor || actor == owner || !actor->IsActive() || actor->IsPendingDestroy()) continue;
		const Entity* entity = dynamic_cast<const Entity*>(actor);
		if (entity && entity->IsDead()) continue;

		Vector3 difference = actor->transform.position - ownerTransform->position;
		difference.y = 0.0f;
		const float distanceSquared = difference.LengthSquared();
		if (distanceSquared > nearestDistanceSquared) continue;
		nearestDistanceSquared = distanceSquared;
		nearest = actor;
	}

	return nearest;
}

bool LockOnComponent::LockOnNearestEnemy()
{
	Actor* candidate = GetAttackCandidate();
	if (!candidate) { ClearTarget(); return false; }
	LockOn(candidate);
	return true;
}

// 対象から明確に離れる移動入力が入ったら、攻撃の吸い付きを解除する
void LockOnComponent::ReleaseIfMovingAway(const Vector3& worldMoveDirection)
{
	if (!target) return;
	if (!IsTargetValid())
	{
		ClearTarget();
		return;
	}

	Vector3 moveDirection = worldMoveDirection;
	moveDirection.y = 0.0f;
	if (moveDirection.LengthSquared() <= eps) return;
	moveDirection.Normalize();

	Transform* ownerTransform = owner ? owner->GetTransform() : nullptr;
	if (!ownerTransform) return;
	Vector3 directionToTarget = target->transform.position - ownerTransform->position;
	directionToTarget.y = 0.0f;
	if (directionToTarget.LengthSquared() <= eps) return;
	directionToTarget.Normalize();

	if (moveDirection.Dot(directionToTarget) < releaseMoveDot) ClearTarget();
}

void LockOnComponent::ClearTarget()
{
	target = nullptr;
	targetLifetime.reset();
}

void LockOnComponent::PauseRotation(float duration)
{
	rotationPauseTimer.Extend(duration);
}

bool LockOnComponent::IsTargetValid() const
{
	const auto actorOwner = targetLifetime.lock();
	if (!actorOwner || actorOwner.get() != target) return false;
	if (!ownerEntity || ownerEntity->IsDead() || !target->IsActive() || target->IsPendingDestroy()) return false;

	const Entity* entity = dynamic_cast<const Entity*>(target);
	return !entity || !entity->IsDead();
}
