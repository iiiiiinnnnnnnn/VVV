// NavMeshAgent.cpp

#include "Physics/Navigation/NavMeshAgent.h"

#include "Gameplay/Actor/Actor.h"
#include "Gameplay/Actor/ActorManager.h"
#include "Physics/Collider/CharacterController.h"
#include "Application/Time/GameTime.h"
#include "Physics/Navigation/NavMeshActor.h"

#include "imgui.h"

NavMeshAgent::NavMeshAgent(Object* owner)
	: Component(owner)
{
}

void NavMeshAgent::Update()
{
	lastMoveDelta = Vector3::Zero;

	if (!autoMove) return;

	Actor* actor = dynamic_cast<Actor*>(owner);
	if (!actor) return;

	if (!characterController)
		characterController = actor->GetComponent<CharacterController>();

	if (!characterController)
	{
		statusMessage = "CharacterController not found.";
		return;
	}

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
	pathFailTimer = 0.0f;
	hasLastNextPoint = false;
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
	pathFailTimer = 0.0f;
	hasLastNextPoint = false;
	statusMessage = "Random destination selected.";
	return true;
}

void NavMeshAgent::Stop()
{
	currentSpeed = 0.0f;
	currentTurnAngle = 0.0f;
	lastMoveDelta = Vector3::Zero;
	pathFailTimer = 0.0f;
	hasLastNextPoint = false;
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

	const Vector3 toTarget = targetPosition - actor->transform.position;
	Vector3 flatToTarget = toTarget;
	flatToTarget.y = 0.0f;

	Vector3 nextPoint;
	bool directMove = false;
	NavMeshActor* navMeshActor = NavMeshActor::GetActive();
	if (navMeshActor)
	{
		Vector3 nearestPoint;
		if (navMeshActor->FindNearestPoint(
				actor->transform.position,
				nearestPoint))
		{
			Vector3 recoveryDirection =
				nearestPoint - actor->transform.position;
			recoveryDirection.y = 0.0f;
			const float recoveryDistance =
				recoveryDirection.Length();
			constexpr float recoveryThreshold = 0.005f;
			if (recoveryDistance > recoveryThreshold)
			{
				recoveryDirection /= recoveryDistance;
				currentTurnAngle = atan2f(
					recoveryDirection.Dot(actor->transform.right),
					recoveryDirection.Dot(actor->transform.forward));

				if (rotateToMoveDirection)
				{
					const float targetYaw = atan2f(
						recoveryDirection.x,
						recoveryDirection.z);
					const Quaternion targetRotation =
						Quaternion::CreateFromYawPitchRoll(
							targetYaw,
							0.0f,
							0.0f);
					const float rate =
						1.0f -
						expf(-turnSpeed * Game::Time::deltaTime);
					actor->transform.SetRotation(
						Quaternion::Slerp(
							actor->transform.rotation,
							targetRotation,
							rate));
				}

				if (movementPaused)
				{
					currentSpeed = 0.0f;
					statusMessage = "Turning to NavMesh.";
					return false;
				}

				const float moveDistance = std::min(
					speed * Game::Time::deltaTime,
					recoveryDistance);
				lastMoveDelta =
					recoveryDirection * moveDistance;
				currentSpeed = speed;
				characterController->Move(lastMoveDelta);

				pathFailTimer = 0.0f;
				hasLastNextPoint = false;
				statusMessage = "Returning to NavMesh.";
				return false;
			}
		}
	}

	if (flatToTarget.LengthSquared() <=
		stoppingDistance * stoppingDistance)
	{
		statusMessage = "Arrived.";
		currentSpeed = 0.0f;
		return true;
	}

	if (!navMeshActor)
	{
		if (!directMoveOnPathFail)
		{
			statusMessage = "NavMeshActor not found.";
			currentSpeed = 0.0f;
			return false;
		}

			nextPoint = targetPosition;
		directMove = true;
		statusMessage = "Direct move: NavMeshActor not found.";
	}
	else if (!navMeshActor->FindNextPoint(actor->transform.position, targetPosition, nextPoint))
	{
		pathFailTimer += Game::Time::deltaTime;
		if (!useLastValidPathOnFail || !hasLastNextPoint || pathFailTimer > pathFailGraceTime)
		{
			if (navMeshActor->FindObstacleDetourPoint(
				actor->transform.position,
				targetPosition,
				nextPoint))
			{
				directMove = true;
				statusMessage = "Local obstacle detour.";
			}
			else if (directMoveOnPathFail &&
				!navMeshActor->IsDirectPathBlocked(
					actor->transform.position,
					targetPosition))
			{
				nextPoint = targetPosition;
				directMove = true;
				statusMessage = "Direct move: Path not found.";
			}
			else
			{
				statusMessage = "Path blocked by obstacle.";
				currentSpeed = 0.0f;
				return false;
			}
		}
		else
		{
			nextPoint = lastNextPoint;
			statusMessage = "Using last path.";
		}
	}
	else
	{
		pathFailTimer = 0.0f;
	}

	Vector3 direction = nextPoint - actor->transform.position;
	direction.y = 0.0f;

	if (direction.LengthSquared() <= eps)
	{
		if (navMeshActor && navMeshActor->IsDirectPathBlocked(
			actor->transform.position,
			targetPosition))
		{
			statusMessage = "Next point too close and direct path blocked.";
			currentSpeed = 0.0f;
			return false;
		}

		nextPoint = targetPosition;
		direction = flatToTarget;
		directMove = true;
		statusMessage = "Direct move: next point too close.";
	}

	if (!directMove && pathFailTimer <= 0.0f)
	{
		lastNextPoint = nextPoint;
		hasLastNextPoint = true;
	}

	const float nextPointDistance = direction.Length();
	direction /= nextPointDistance;
	currentTurnAngle = atan2f(
		direction.Dot(actor->transform.right),
		direction.Dot(actor->transform.forward));

	if (rotateToMoveDirection)
	{
		const float targetYaw = atan2f(direction.x, direction.z);
		const Quaternion targetRotation =
			Quaternion::CreateFromYawPitchRoll(targetYaw, 0.0f, 0.0f);
		const float rate = 1.0f - expf(-turnSpeed * Game::Time::deltaTime);
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
	characterController->Move(lastMoveDelta);

	if (!directMove && pathFailTimer <= 0.0f)
		statusMessage = "Moving.";

	return false;
}

void NavMeshAgent::DrawGUI()
{
	ImGui::Checkbox("Auto Move", &autoMove);
	ImGui::Checkbox("Chase Target Tag", &chaseTargetTag);
	ImGui::InputText("Target Tag", &targetTag);
	ImGui::Checkbox("Rotate To Move Direction", &rotateToMoveDirection);
	ImGui::Checkbox("Movement Paused", &movementPaused);
	ImGui::Checkbox("Use Last Valid Path On Fail", &useLastValidPathOnFail);
	ImGui::Checkbox("Direct Move On Path Fail", &directMoveOnPathFail);
	ImGui::DragFloat("Speed", &speed, 0.1f, 0.0f, 30.0f);
	ImGui::DragFloat("Stopping Distance", &stoppingDistance, 0.1f, 0.0f, 20.0f);
	ImGui::DragFloat("Path Fail Grace Time", &pathFailGraceTime, 0.01f, 0.0f, 5.0f);
	ImGui::DragFloat("Turn Speed", &turnSpeed, 0.1f, 0.0f, 30.0f);
	ImGui::DragFloat("Random Min Distance", &randomMinDistance, 0.1f, 0.0f, 100.0f);
	ImGui::DragFloat("Random Max Distance", &randomMaxDistance, 0.1f, 0.0f, 100.0f);
	if (ImGui::Button("Move To Random Position"))
		MoveToRandomPosition(randomMinDistance, randomMaxDistance);
	ImGui::Text("%s", statusMessage.c_str());
}
