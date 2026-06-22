// NavMeshAgent.cpp

#include "NavMeshAgent.h"

#include "Actor.h"
#include "ActorManager.h"
#include "CharacterController.h"
#include "GameTime.h"
#include "NavMeshActor.h"

#include "imgui.h"

NavMeshAgent::NavMeshAgent(Object* owner)
	: Component(owner)
{
}

void NavMeshAgent::Update()
{
	if (!autoMove) return;

	Actor* actor = GetOwnerAsActor();
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
	Actor* actor = GetOwnerAsActor();
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
	pathFailTimer = 0.0f;
	hasLastNextPoint = false;
	statusMessage = "Idle.";
}

Actor* NavMeshAgent::FindTargetByTag()
{
	Actor* actor = GetOwnerAsActor();
	ActorManager* actorManager = actor ? actor->GetActorManager() : nullptr;
	if (!actorManager) return nullptr;

	for (const std::shared_ptr<Actor>& targetActor : actorManager->GetActors())
	{
		if (!targetActor || targetActor->IsPendingDestroy()) continue;
		if (!targetActor->CompareTag(targetTag)) continue;
		return targetActor.get();
	}

	return nullptr;
}

void NavMeshAgent::MoveToTarget(Actor* actor, Actor* targetActor)
{
	const Vector3 toTarget = targetActor->transform.position - actor->transform.position;
	Vector3 flatToTarget = toTarget;
	flatToTarget.y = 0.0f;

	if (flatToTarget.LengthSquared() <= stoppingDistance * stoppingDistance)
	{
		statusMessage = "Arrived.";
		return;
	}

	NavMeshActor* navMeshActor = NavMeshActor::GetActive();
	if (!navMeshActor)
	{
		statusMessage = "NavMeshActor not found.";
		return;
	}

	Vector3 nextPoint;
	if (!navMeshActor->FindNextPoint(actor->transform.position, targetActor->transform.position, nextPoint))
	{
		pathFailTimer += Game::Time::deltaTime;
		if (!useLastValidPathOnFail || !hasLastNextPoint || pathFailTimer > pathFailGraceTime)
		{
			statusMessage = "Path not found.";
			return;
		}

		nextPoint = lastNextPoint;
		statusMessage = "Using last path.";
	}
	else
	{
		pathFailTimer = 0.0f;
		lastNextPoint = nextPoint;
		hasLastNextPoint = true;
	}

	Vector3 direction = nextPoint - actor->transform.position;
	direction.y = 0.0f;

	if (direction.LengthSquared() <= repathDistance * repathDistance)
	{
		direction = flatToTarget;
	}

	if (direction.LengthSquared() <= eps)
	{
		statusMessage = "Next point too close.";
		return;
	}

	direction.Normalize();
	characterController->Move(direction * speed * Game::Time::deltaTime);

	if (rotateToMoveDirection)
	{
		const float targetYaw = atan2f(direction.x, direction.z);
		const Quaternion targetRotation =
			Quaternion::CreateFromYawPitchRoll(targetYaw, 0.0f, 0.0f);
		const float rate = 1.0f - expf(-turnSpeed * Game::Time::deltaTime);
		actor->transform.SetRotation(
			Quaternion::Slerp(actor->transform.rotation, targetRotation, rate));
	}

	if (pathFailTimer <= 0.0f)
		statusMessage = "Moving.";
}

void NavMeshAgent::DrawGUI()
{
	if (!ImGui::TreeNode("NavMeshAgent"))
		return;

	ImGui::Checkbox("Auto Move", &autoMove);
	ImGui::Checkbox("Chase Target Tag", &chaseTargetTag);
	ImGui::InputText("Target Tag", &targetTag);
	ImGui::Checkbox("Rotate To Move Direction", &rotateToMoveDirection);
	ImGui::Checkbox("Use Last Valid Path On Fail", &useLastValidPathOnFail);
	ImGui::DragFloat("Speed", &speed, 0.1f, 0.0f, 30.0f);
	ImGui::DragFloat("Stopping Distance", &stoppingDistance, 0.1f, 0.0f, 20.0f);
	ImGui::DragFloat("Repath Distance", &repathDistance, 0.01f, 0.0f, 5.0f);
	ImGui::DragFloat("Path Fail Grace Time", &pathFailGraceTime, 0.01f, 0.0f, 5.0f);
	ImGui::DragFloat("Turn Speed", &turnSpeed, 0.1f, 0.0f, 30.0f);
	ImGui::Text("%s", statusMessage.c_str());

	ImGui::TreePop();
}
