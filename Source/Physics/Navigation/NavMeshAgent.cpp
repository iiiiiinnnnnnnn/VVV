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

	Actor* targetActor = target;
	if (chaseTargetTag)
		targetActor = FindTargetByTag();

	if (!targetActor)
	{
		statusMessage = "Target not found.";
		return;
	}

	MoveToTarget(actor, targetActor);
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

	MoveToTarget(actor, targetActor);
}

void NavMeshAgent::Stop()
{
	currentSpeed = 0.0f;
	lastMoveDelta = Vector3::Zero;
	pathFailTimer = 0.0f;
	hasLastNextPoint = false;
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

void NavMeshAgent::MoveToTarget(Actor* actor, Actor* targetActor)
{
	lastMoveDelta = Vector3::Zero;

	const Vector3 toTarget = targetActor->transform.position - actor->transform.position;
	Vector3 flatToTarget = toTarget;
	flatToTarget.y = 0.0f;

	if (flatToTarget.LengthSquared() <= stoppingDistance * stoppingDistance)
	{
		statusMessage = "Arrived.";
		currentSpeed = 0.0f;
		return;
	}

	Vector3 nextPoint;
	bool directMove = false;
	NavMeshActor* navMeshActor = NavMeshActor::GetActive();
	if (!navMeshActor)
	{
		if (!directMoveOnPathFail)
		{
			statusMessage = "NavMeshActor not found.";
			currentSpeed = 0.0f;
			return;
		}

		nextPoint = targetActor->transform.position;
		directMove = true;
		statusMessage = "Direct move: NavMeshActor not found.";
	}
	else if (!navMeshActor->FindNextPoint(actor->transform.position, targetActor->transform.position, nextPoint))
	{
		pathFailTimer += Game::Time::deltaTime;
		if (!useLastValidPathOnFail || !hasLastNextPoint || pathFailTimer > pathFailGraceTime)
		{
			if (!directMoveOnPathFail ||
				navMeshActor->IsDirectPathBlocked(
					actor->transform.position,
					targetActor->transform.position))
			{
				statusMessage = "Path blocked by obstacle.";
				currentSpeed = 0.0f;
				return;
			}

			else
			{
				nextPoint = targetActor->transform.position;
				directMove = true;
				statusMessage = "Direct move: Path not found.";
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
		lastNextPoint = nextPoint;
		hasLastNextPoint = true;
	}

	Vector3 direction = nextPoint - actor->transform.position;
	direction.y = 0.0f;

	if (direction.LengthSquared() <= eps)
	{
		statusMessage = "Next point too close.";
		return;
	}

	const float nextPointDistance = direction.Length();
	direction /= nextPointDistance;
	currentSpeed = speed;
	const float moveDistance = std::min(
		speed * Game::Time::deltaTime,
		nextPointDistance);
	lastMoveDelta = direction * moveDistance;
	characterController->Move(lastMoveDelta);

	if (rotateToMoveDirection)
	{
		const float targetYaw = atan2f(direction.x, direction.z);
		const Quaternion targetRotation =
			Quaternion::CreateFromYawPitchRoll(targetYaw, 0.0f, 0.0f);
		const float rate = 1.0f - expf(-turnSpeed * Game::Time::deltaTime);
		actor->transform.SetRotation(
			Quaternion::Slerp(actor->transform.rotation, targetRotation, rate));
	}

	if (!directMove && pathFailTimer <= 0.0f)
		statusMessage = "Moving.";
}

void NavMeshAgent::DrawGUI()
{
	ImGui::Checkbox("Auto Move", &autoMove);
	ImGui::Checkbox("Chase Target Tag", &chaseTargetTag);
	ImGui::InputText("Target Tag", &targetTag);
	ImGui::Checkbox("Rotate To Move Direction", &rotateToMoveDirection);
	ImGui::Checkbox("Use Last Valid Path On Fail", &useLastValidPathOnFail);
	ImGui::Checkbox("Direct Move On Path Fail", &directMoveOnPathFail);
	ImGui::DragFloat("Speed", &speed, 0.1f, 0.0f, 30.0f);
	ImGui::DragFloat("Stopping Distance", &stoppingDistance, 0.1f, 0.0f, 20.0f);
	ImGui::DragFloat("Path Fail Grace Time", &pathFailGraceTime, 0.01f, 0.0f, 5.0f);
	ImGui::DragFloat("Turn Speed", &turnSpeed, 0.1f, 0.0f, 30.0f);
	ImGui::Text("%s", statusMessage.c_str());
}
