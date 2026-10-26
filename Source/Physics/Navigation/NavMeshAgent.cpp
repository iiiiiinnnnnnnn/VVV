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
	bool directMove = true;
	NavMeshActor* navMeshActor = NavMeshActor::GetActive();

	// 自身がNavMesh内なら経路を使い、外なら目的地へ直線移動する。
	if (!chargeMovement && !directMovement && navMeshActor &&
		!navMeshActor->IsOutsideOrNearBoundary(actor->transform.position, 0.0f))
	{
		directMove = !navMeshActor->FindNextPoint(
			actor->transform.position, targetPosition, nextPoint);
		if (directMove) nextPoint = targetPosition;
	}

	// 到着判定には、AIが決めた元の目的地を使う。
	if (flatToTarget.LengthSquared() <= stoppingDistance * stoppingDistance)
	{
		statusMessage = "Arrived.";
		currentSpeed = 0.0f;
		return true;
	}

	Vector3 direction = nextPoint - actor->transform.position;
	direction.y = 0.0f;

	// 経路の端まで来ても目的地には届いていなければ、そのまま外へ進む。
	if (direction.LengthSquared() <= 0.01f)
	{
		direction = flatToTarget;
		directMove = true;
	}

	const float nextPointDistance = direction.Length();
	if (!IsFinite(nextPoint) || !std::isfinite(nextPointDistance) || nextPointDistance <= eps)
	{
		statusMessage = "Movement stopped: NavMesh returned an invalid direction.";
		currentSpeed = 0.0f;
		return false;
	}
	direction /= nextPointDistance;
	currentTurnAngle = atan2f(
		direction.Dot(actor->transform.GetRight()),
		direction.Dot(actor->transform.GetForward()));

	if (rotateToMoveDirection && !chargeMovement && !rotationPaused)
	{
		const float targetYaw = atan2f(direction.x, direction.z);
		const Quaternion targetRotation =
			Quaternion::CreateFromYawPitchRoll(targetYaw, 0.0f, 0.0f);
		const float rate = angularSpeed > 0.0f
			? std::min(1.0f, RAD(angularSpeed) * Game::Time::deltaTime / std::max(fabsf(currentTurnAngle), eps))
			: 1.0f - expf(-turnSpeed * Game::Time::deltaTime);
		actor->transform.SetRotation(
			Quaternion::Slerp(actor->transform.rotation, targetRotation, rate));
	}

	if (movementPaused)
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

	// Move first so contact callbacks run before the charge stops.
	characterController->Move(lastMoveDelta);
	if (chargeMovement && characterController->HitSideOnLastMove())
	{
		Stop();
		statusMessage = "Charge stopped: Collision.";
		return true;
	}

	statusMessage = directMove ? "Direct move." : "NavMesh move.";

	return false;
}

void NavMeshAgent::DrawGUI()
{
	ImGui::Checkbox("Auto Move", &autoMove);
	ImGui::Checkbox("Chase Target Tag", &chaseTargetTag);
	ImGui::InputText("Target Tag", &targetTag);
	ImGui::Checkbox("Rotate To Move Direction", &rotateToMoveDirection);
	ImGui::Checkbox("Movement Paused", &movementPaused);
	ImGui::DragFloat("Speed", &speed, 0.1f, 0.0f, 30.0f);
	ImGui::DragFloat("Stopping Distance", &stoppingDistance, 0.1f, 0.0f, 20.0f);
	ImGui::DragFloat("Turn Speed", &turnSpeed, 0.1f, 0.0f, 30.0f);
	ImGui::DragFloat("Random Min Distance", &randomMinDistance, 0.1f, 0.0f, 100.0f);
	ImGui::DragFloat("Random Max Distance", &randomMaxDistance, 0.1f, 0.0f, 100.0f);
	if (ImGui::Button("Move To Random Position"))
		MoveToRandomPosition(randomMinDistance, randomMaxDistance);
	ImGui::Text("%s", statusMessage.c_str());
}
