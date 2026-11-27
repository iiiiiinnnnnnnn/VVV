#include <ProfileKit/ProfileKit.h>
#include <ImGuiToolKit/Drag.h>
#include "Physics/Navigation/NavMeshAgent.h"

#include "Gameplay/Actor/Actor.h"
#include "Gameplay/Actor/ActorManager.h"
#include "Physics/Collider/CharacterController.h"
#include "Application/Time/GameTime.h"
#include "Physics/Navigation/NavMeshActor.h"

#include "imgui.h"

#include <cmath>

static bool IsFinite(const Vector3& value)
{
	return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

NavMeshAgent::NavMeshAgent(Object* owner)
	: Component(owner)
{
}

void NavMeshAgent::Update()
{
	if (manualUpdate) return;
	UpdateMovement();
}

void NavMeshAgent::UpdateMovement()
{
	PROFILE_FUNCTION();
	lastMoveDelta = Vector3::Zero;

	Actor* actor = dynamic_cast<Actor*>(owner);
	if (!actor) return;

	if (!characterController)
		characterController = actor->GetComponent<CharacterController>();

	if (!characterController)
	{
		statusMessage = "CharacterController not found.";
		return;
	}

	if (!autoMove) return;

	if (hasDestination)
	{
		if (MoveToPosition(actor, destination))
		{
			hasDestination = false;
			autoMove = false;
		}
		return;
	}

	Actor* targetActor = target;
	if (chaseTargetTag)
		targetActor = FindTargetByTag();

	if (!targetActor)
	{
		statusMessage = "Target not found.";
		return;
	}

	MoveToPosition(actor, targetActor->transform.position);
}

void NavMeshAgent::MoveToTarget(Actor* targetActor)
{
	lastMoveDelta = Vector3::Zero;

	Actor* actor = dynamic_cast<Actor*>(owner);
	if (!actor) return;

	if (!characterController)
		characterController = actor->GetComponent<CharacterController>();

	if (!characterController)
	{
		statusMessage = "CharacterController not found.";
		return;
	}

	if (!targetActor)
	{
		statusMessage = "Target not found.";
		return;
	}

	MoveToPosition(actor, targetActor->transform.position);
}

void NavMeshAgent::MoveToPosition(const Vector3& targetPosition)
{
	if (chargeMovement)
	{
		chargeDirection = targetPosition - owner->GetTransform()->position;
		chargeDirection.y = 0.0f;
		if (chargeDirection.LengthSquared() > eps) chargeDirection.Normalize();
	}
	destination = targetPosition;
	hasDestination = true;
	autoMove = true;
	statusMessage = "Destination selected.";
}

bool NavMeshAgent::MoveToRandomPosition(float minDistance, float maxDistance)
{
	Actor* actor = dynamic_cast<Actor*>(owner);
	if (!actor)
	{
		statusMessage = "Owner is not Actor.";
		return false;
	}

	NavMeshActor* navMeshActor = NavMeshActor::GetActive();
	if (!navMeshActor)
	{
		statusMessage = "NavMeshActor not found.";
		return false;
	}

	Vector3 randomPoint;
	if (!navMeshActor->FindRandomPoint(
		actor->transform.position,
		minDistance,
		maxDistance,
		randomPoint))
	{
		statusMessage = "Random point not found.";
		return false;
	}

	destination = randomPoint;
	hasDestination = true;
	autoMove = true;
	statusMessage = "Random destination selected.";
	return true;
}

void NavMeshAgent::Stop()
{
	currentSpeed = 0.0f;
	currentTurnAngle = 0.0f;
	turning = false;
	lastMoveDelta = Vector3::Zero;
	hasDestination = false;
	autoMove = false;
	statusMessage = "Idle.";
}

Actor* NavMeshAgent::FindTargetByTag()
{
	ActorManager* actorManager = ActorManager::GetActive();
	if (!actorManager) return nullptr;

	for (Actor* targetActor : actorManager->GetActors())
	{
		if (!targetActor || targetActor->IsPendingDestroy()) continue;
		if (!targetActor->CompareTag(targetTag)) continue;
		return targetActor;
	}

	return nullptr;
}

bool NavMeshAgent::MoveToPosition(Actor* actor, const Vector3& targetPosition)
{
	lastMoveDelta = Vector3::Zero;
	currentTurnAngle = 0.0f;
	turning = false;
	if (!actor || !IsFinite(actor->transform.position) || !IsFinite(targetPosition))
	{
		statusMessage = "Movement stopped: Position is invalid.";
		currentSpeed = 0.0f;
		return false;
	}

	const Vector3 toTarget = targetPosition - actor->transform.position;
	Vector3 flatToTarget = toTarget;
	flatToTarget.y = 0.0f;

	Vector3 nextPoint = targetPosition;
	bool directMove = chargeMovement || directMovement;
	bool recovering = false;
	NavMeshActor* navMeshActor = NavMeshActor::GetActive();

	if (!directMove)
	{
		if (stayOnNavMesh && !navMeshActor)
		{
			currentSpeed = 0.0f;
			statusMessage = "NavMesh unavailable.";
			return false;
		}

		const bool outside = !navMeshActor || navMeshActor->IsOutsideOrNearBoundary(actor->transform.position, 0.0f);
		if (stayOnNavMesh && outside)
		{
			if (!navMeshActor->FindRecoveryPoint(actor->transform.position, characterController->GetRadius(), nextPoint))
			{
				currentSpeed = 0.0f;
				statusMessage = "NavMesh recovery point not found.";
				return false;
			}
			recovering = true;
		}
		else if (!outside)
		{
			const float goalSearchDistance = stayOnNavMesh ? flatToTarget.Length() : 0.0f;
			if (!navMeshActor->FindNextPoint(
				actor->transform.position, targetPosition, nextPoint, nullptr, goalSearchDistance))
			{
				currentSpeed = 0.0f;
				statusMessage = "NavMesh path not found.";
				return false;
			}
		}
		else
		{
			directMove = true;
		}
	}

	// 到着判定には、AIが決めた元の目的地を使う。
	if (!recovering && flatToTarget.LengthSquared() <= stoppingDistance * stoppingDistance)
	{
		statusMessage = "Arrived.";
		currentSpeed = 0.0f;
		return true;
	}

	Vector3 direction = nextPoint - actor->transform.position;
	direction.y = 0.0f;

	// 部分経路の終端では止まり、Recastが歩行不可にした領域へ直進しない。
	if (direction.LengthSquared() <= 0.01f && !directMove && !recovering)
	{
		statusMessage = "NavMesh path end reached.";
		currentSpeed = 0.0f;
		return false;
	}

	const float nextPointDistance = direction.Length();
	if (!IsFinite(nextPoint) || !std::isfinite(nextPointDistance) || nextPointDistance <= eps)
	{
		statusMessage = "Movement stopped: NavMesh returned an invalid direction.";
		currentSpeed = 0.0f;
		return false;
	}
	direction /= nextPointDistance;
	if (chargeMovement) direction = chargeDirection;
	RotateTowards(actor, direction);
	turning = manualUpdate && !chargeMovement && std::abs(currentTurnAngle) > turnThreshold;

	if (movementPaused || turning)
	{
		currentSpeed = 0.0f;
		statusMessage = "Turning.";
		return false;
	}

	currentSpeed = speed;
	const float moveDistance = std::min(
		speed * Game::Time::deltaTime,
		nextPointDistance);
	lastMoveDelta = direction * moveDistance;

	// 通常歩行は歩行面内に収め、復帰中だけは最寄りの内側へ物理移動する。
	if (!directMove && !recovering && navMeshActor)
	{
		const Vector3 start = actor->transform.position;
		const Vector3 end = start + lastMoveDelta;

		Vector3 resultPosition;

		if (!navMeshActor->MoveAlongSurface(start, end, resultPosition))
		{
			currentSpeed = 0.0f;
			lastMoveDelta = Vector3::Zero;
			return false;
		}

		lastMoveDelta = resultPosition - start;
		lastMoveDelta.y = 0.0f;
	}

	if (!IsFinite(lastMoveDelta))
	{
		statusMessage = "Movement stopped: Movement delta is invalid.";
		currentSpeed = 0.0f;
		lastMoveDelta = Vector3::Zero;
		return false;
	}

	// 突進は迂回せず直進し、崖や接触した障害物で終了する。
	if (chargeMovement && navMeshActor &&
		navMeshActor->IsCliffAlongSegment(actor->transform.position,
			actor->transform.position + lastMoveDelta, 45.0f))
	{
		Stop();
		statusMessage = "Charge stopped: Cliff.";
		return true;
	}

	characterController->Move(lastMoveDelta);
	if (chargeMovement && characterController->HitSideOnLastMove())
	{
		Stop();
		statusMessage = "Charge stopped: Collision.";
		return true;
	}

	statusMessage = directMove ? "Direct move."
		: recovering ? "Recovering to NavMesh." : "NavMesh move.";

	return false;
}

void NavMeshAgent::RotateTowards(Actor* actor, const Vector3& direction)
{
	const float side = direction.Dot(actor->transform.GetRight());
	const float forward = direction.Dot(actor->transform.GetForward());
	currentTurnAngle = atan2f(side, forward);

	if (!rotateToMoveDirection || chargeMovement || rotationPaused) return;

	const float targetYaw = atan2f(direction.x, direction.z);
	const Quaternion targetRotation = Quaternion::CreateFromYawPitchRoll(targetYaw, 0.0f, 0.0f);
	float rate;
	if (angularSpeed > 0.0f)
	{
		const float turnAmount = RAD(angularSpeed) * Game::Time::deltaTime;
		rate = std::min(1.0f, turnAmount / std::max(std::abs(currentTurnAngle), eps));
	}
	else
	{
		const float followTime = turnSpeed * Game::Time::deltaTime;
		rate = 1.0f - expf(-followTime);
	}
	const Quaternion rotation = Quaternion::Slerp(actor->transform.rotation, targetRotation, rate);
	actor->transform.SetRotation(rotation);

	// 手動更新では、このフレームの旋回後の角度で歩行開始を判断する。
	// 自動更新を利用する既存の敵の判定タイミングは変えない。
	if (manualUpdate)
	{
		const float remainingSide = direction.Dot(actor->transform.GetRight());
		const float remainingForward = direction.Dot(actor->transform.GetForward());
		currentTurnAngle = atan2f(remainingSide, remainingForward);
	}
}

bool NavMeshAgent::FacePosition(const Vector3& position)
{
	lastMoveDelta = Vector3::Zero;
	currentSpeed = 0.0f;
	currentTurnAngle = 0.0f;
	turning = false;

	Actor* actor = dynamic_cast<Actor*>(owner);
	if (!actor) return false;

	Vector3 direction = position - actor->transform.position;
	direction.y = 0.0f;
	if (direction.LengthSquared() <= eps) return true;
	direction.Normalize();
	RotateTowards(actor, direction);
	turning = std::abs(currentTurnAngle) > turnThreshold;
	statusMessage = turning ? "Turning." : "Facing target.";
	return !turning;
}

void NavMeshAgent::DrawGUI()
{
	ImGui::Checkbox("Auto Move", &autoMove);
	ImGui::Checkbox("Chase Target Tag", &chaseTargetTag);
	ImGui::InputText("Target Tag", &targetTag);
	ImGui::Checkbox("Rotate To Move Direction", &rotateToMoveDirection);
	ImGui::Checkbox("Movement Paused", &movementPaused);
	ImGuiToolKit::DragSpeedFloat("Speed", &speed, 0.1f, 0.0f, 30.0f);
	ImGui::DragFloat("Stopping Distance", &stoppingDistance, 0.1f, 0.0f, 20.0f);
	ImGuiToolKit::DragSpeedFloat("Turn Speed", &turnSpeed, 0.1f, 0.0f, 30.0f);
	ImGui::DragFloat("Random Min Distance", &randomMinDistance, 0.1f, 0.0f, 100.0f);
	ImGui::DragFloat("Random Max Distance", &randomMaxDistance, 0.1f, 0.0f, 100.0f);
	if (ImGui::Button("Move To Random Position"))
		MoveToRandomPosition(randomMinDistance, randomMaxDistance);
	ImGui::Text("%s", statusMessage.c_str());
}
