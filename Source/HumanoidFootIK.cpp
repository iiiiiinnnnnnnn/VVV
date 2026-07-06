// HumanoidFootIK.cpp

#include "HumanoidFootIK.h"

#include "Animator.h"
#include "GameTime.h"
#include "Object.h"

using std::max;
using std::min;

HumanoidFootIK::HumanoidFootIK(
	Object* owner,
	LayerId layerId,
	Model* model,
	Animator* animator,
	const char* activeStateName,
	const char* pelvisName,
	const char* thighLName, const char* calfLName, const char* footLName, const char* ballLName,
	const char* thighRName, const char* calfRName, const char* footRName, const char* ballRName)
	: Component(owner), model(model), animator(animator)
{
	if (activeStateName)
	{
		this->activeStateName = activeStateName;
	}

	if (model && pelvisName)
	{
		hipNodeIndex = model->GetNodeIndex(pelvisName);
	}

	if (owner && model)
	{
		footIK_L = owner->AddComponent<FootIK>(
			layerId,
			model,
			thighLName,
			calfLName,
			footLName,
			ballLName);

		footIK_R = owner->AddComponent<FootIK>(
			layerId,
			model,
			thighRName,
			calfRName,
			footRName,
			ballRName);
	}
}

void HumanoidFootIK::LateUpdate()
{
	if (!model) return;

	Vector3 baseHipLocalPosition = Vector3::Zero;
	if (hipNodeIndex >= 0)
	{
		baseHipLocalPosition = model->GetNodes()[hipNodeIndex].position;
	}

	if (!ShouldUseIK())
	{
		ResetHipOffset(baseHipLocalPosition);
		return;
	}

	if (footIK_L && footIK_L->IsIKEnabled())
	{
		footIK_L->UpdateGroundTarget(rayUp, rayDown, contactOffset);
	}

	if (footIK_R && footIK_R->IsIKEnabled())
	{
		footIK_R->UpdateGroundTarget(rayUp, rayDown, contactOffset);
	}

	ApplyHipOffset(baseHipLocalPosition);

	if (footIK_L && footIK_L->IsIKEnabled())
	{
		footIK_L->SolveIK(model->GetWorldTransform());
	}

	if (footIK_R && footIK_R->IsIKEnabled())
	{
		footIK_R->SolveIK(model->GetWorldTransform());
	}
}

void HumanoidFootIK::DrawGUI()
{
	ImGui::DragFloat("Ray Up", &rayUp, 0.01f, 0.0f, 3.0f);
	ImGui::DragFloat("Ray Down", &rayDown, 0.01f, 0.0f, 5.0f);
	ImGui::DragFloat("Contact Offset", &contactOffset, 0.001f, 0.0f, 0.2f);
	ImGui::DragFloat("Visual Hip Offset Y", &visualHipOffsetY, 0.01f, -1.0f, 1.0f);
	ImGui::DragFloat("Min Hip Offset Y", &minHipOffsetY, 0.01f, -1.0f, 0.0f);
	ImGui::DragFloat("Max Hip Offset Y", &maxHipOffsetY, 0.01f, -1.0f, 1.0f);
}

void HumanoidFootIK::SetRay(float up, float down, float contactOffset)
{
	rayUp = up;
	rayDown = down;
	this->contactOffset = contactOffset;
}

void HumanoidFootIK::SetHipOffsetLimit(float minOffsetY, float maxOffsetY)
{
	minHipOffsetY = minOffsetY;
	maxHipOffsetY = maxOffsetY;
}

bool HumanoidFootIK::ShouldUseIK() const
{
	if (!animator) return true;
	return animator->GetCurrentStateName(0) == activeStateName;
}

void HumanoidFootIK::ApplyHipOffset(const Vector3& baseHipLocalPosition)
{
	float targetOffsetY = 0.0f;
	float highestFootOffsetY = 0.0f;

	if (footIK_L &&
		footIK_L->IsIKEnabled() &&
		footIK_L->HasGroundContact())
	{
		const float footOffsetY = footIK_L->GetGroundOffsetY();

		targetOffsetY = min(targetOffsetY, footOffsetY);
		highestFootOffsetY = max(highestFootOffsetY, footOffsetY);
	}

	if (footIK_R &&
		footIK_R->IsIKEnabled() &&
		footIK_R->HasGroundContact())
	{
		const float footOffsetY = footIK_R->GetGroundOffsetY();

		targetOffsetY = min(targetOffsetY, footOffsetY);
		highestFootOffsetY = max(highestFootOffsetY, footOffsetY);
	}

	if (highestFootOffsetY > 0.0f)
	{
		targetOffsetY = min(targetOffsetY, -highestFootOffsetY);
	}

	targetOffsetY = std::clamp(targetOffsetY, minHipOffsetY, maxHipOffsetY);

	const float t = 1.0f - expf(-14.0f * Game::Time::deltaTime);

	visualHipOffsetY += (targetOffsetY - visualHipOffsetY) * t;

	if (hipNodeIndex >= 0)
	{
		model->GetNodes()[hipNodeIndex].position =
			baseHipLocalPosition + Vector3(0.0f, visualHipOffsetY, 0.0f);
	}

	model->UpdateTransform(model->GetWorldTransform());
}

void HumanoidFootIK::ResetHipOffset(const Vector3& baseHipLocalPosition)
{
	visualHipOffsetY = 0.0f;

	if (hipNodeIndex >= 0)
	{
		model->GetNodes()[hipNodeIndex].position = baseHipLocalPosition;
	}

	model->UpdateTransform(model->GetWorldTransform());
}
