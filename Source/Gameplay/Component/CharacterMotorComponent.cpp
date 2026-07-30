// CharacterMotorComponent.cpp

#include "Gameplay/Component/CharacterMotorComponent.h"

#include "Animation/Animator.h"
#include "Application/Time/GameTime.h"
#include "Gameplay/Actor/Actor.h"
#include "Physics/Collider/CharacterController.h"
#include "Physics/Core/PhysicsManager.h"

CharacterMotorComponent::CharacterMotorComponent(
	Object* owner,
	Animator* animator,
	CharacterController* characterController)
	: Component(owner)
	, animator(animator)
	, characterController(characterController)
{
	if (characterController) characterController->SetUseGravity(false);
}

void CharacterMotorComponent::OnUpdate()
{
	Actor* actor = dynamic_cast<Actor*>(owner);
	if (!actor || !animator || !characterController) return;

	rootMotionDelta = Vector3::Zero;
	if (useRootMotion)
	{
		const Quaternion rootMotionRotation = animator->GetRootMotionRot();
		actor->transform.SetRotation(actor->transform.rotation * rootMotionRotation);

		const Vector3 localRootMotionDelta = animator->GetRootMotionVec();
		rootMotionDelta = Vector3::Transform(localRootMotionDelta, actor->transform.rotation);
	}
	UpdateGravity();
	const Vector3 motorVelocity = externalVelocity + Vector3(0.0f, verticalVelocity, 0.0f);
	lastMoveDelta = rootMotionDelta + motorVelocity * Game::Time::deltaTime;
	characterController->Move(lastMoveDelta);
	SnapToGroundIfNeeded();
}

void CharacterMotorComponent::OnDrawGUI()
{
	ImGui::Text(
		"Root Motion Delta: %.3f, %.3f, %.3f",
		rootMotionDelta.x,
		rootMotionDelta.y,
		rootMotionDelta.z);
	ImGui::Text(
		"External Velocity: %.3f, %.3f, %.3f",
		externalVelocity.x,
		externalVelocity.y,
		externalVelocity.z);
	ImGui::Text(
		"Move Delta: %.3f, %.3f, %.3f",
		lastMoveDelta.x,
		lastMoveDelta.y,
		lastMoveDelta.z);
	ImGui::Text("Grounded: %s", grounded ? "true" : "false");
	ImGui::Text("Vertical Velocity: %.3f", verticalVelocity);
	ImGui::Checkbox("Use Root Motion", &useRootMotion);
	ImGui::Checkbox("Use Gravity", &useGravity);
	ImGui::DragFloat("Gravity", &gravity, 0.01f, -100.0f, 100.0f);
	ImGui::Checkbox("Use Ground Snap", &useGroundSnap);
	ImGui::DragFloat("Ground Snap Up", &groundSnapUpDistance, 0.01f, 0.0f, 2.0f);
	ImGui::DragFloat("Ground Snap Down", &groundSnapDownDistance, 0.01f, 0.0f, 3.0f);
	ImGui::DragFloat("Ground Snap Speed", &groundSnapSpeed, 0.1f, 0.0f, 100.0f);
	ImGui::DragFloat("Minimum Ground Normal Y", &minimumGroundNormalY, 0.01f, 0.0f, 1.0f);
}

void CharacterMotorComponent::SetRootMotionNode(const std::string& nodeName)
{
	if (animator) animator->SetRootMotion(nodeName);
}

void CharacterMotorComponent::SetGroundSnapDistance(float upDistance, float downDistance)
{
	groundSnapUpDistance = std::max(upDistance, 0.0f);
	groundSnapDownDistance = std::max(downDistance, 0.0f);
}

bool CharacterMotorComponent::RaycastGround(Vector3& position, Vector3& normal) const
{
	const Actor* actor = dynamic_cast<const Actor*>(owner);
	if (!actor || !characterController) return false;

	PhysicsManager::PhysicsRaycastHit hit;
	const Vector3 rayStart =
		actor->transform.position + Vector3(0.0f, groundSnapUpDistance, 0.0f);
	const float rayDistance = groundSnapUpDistance + groundSnapDownDistance;
	if (!PhysicsManager::Instance().Raycast(
		rayStart,
		Vector3::Down,
		rayDistance,
		hit,
		characterController->GetLayerId(),
		actor)) return false;

	position = hit.position;
	normal = hit.normal;
	return true;
}

void CharacterMotorComponent::UpdateGravity()
{
	Vector3 groundPosition;
	Vector3 groundNormal;
	grounded = RaycastGround(groundPosition, groundNormal) &&
		groundNormal.y >= minimumGroundNormalY;

	if (!useGravity) return;
	if (grounded && verticalVelocity <= 0.0f)
	{
		verticalVelocity = 0.0f;
		return;
	}

	verticalVelocity += gravity * Game::Time::deltaTime;
}

void CharacterMotorComponent::SnapToGroundIfNeeded()
{
	if (!useGroundSnap || verticalVelocity > 0.0f) return;

	Actor* actor = dynamic_cast<Actor*>(owner);
	if (!actor || !characterController) return;

	Vector3 groundPosition;
	Vector3 groundNormal;
	grounded = RaycastGround(groundPosition, groundNormal) &&
		groundNormal.y >= minimumGroundNormalY;
	if (!grounded) return;

	Vector3 targetPosition = actor->transform.position;
	targetPosition.y = groundPosition.y;
	float rate = 1.0f - expf(-groundSnapSpeed * Game::Time::deltaTime);
	rate = std::clamp(rate, 0.0f, 1.0f);
	actor->transform.SetPosition(Vector3::Lerp(actor->transform.position, targetPosition, rate));
	characterController->SetPosition(actor->transform.position);
	verticalVelocity = 0.0f;
}
