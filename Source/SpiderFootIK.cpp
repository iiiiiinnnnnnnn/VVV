// SpiderFootIK.cpp

#include "SpiderFootIK.h"

#include "Actor.h"
#include "Object.h"

SpiderFootIK::SpiderFootIK(Object* owner, LayerId layerId, Model* model)
	: Component(owner), model(model), layerId(layerId)
{
	if (model)
	{
		waistNodeIndex = model->GetNodeIndex("Dummy02");
	}

	AddLeg("Box09", "Box10", "Box11");
	AddLeg("Box20", "Box18", "Box19");
	AddLeg("Box25", "Box22", "Box23");
	AddLeg("Box26", "Box21", "Box24");
	AddLeg("Box31", "Box28", "Box29");
	AddLeg("Box35", "Box32", "Box36");
	AddLeg("Box37", "Box33", "Box34");
	AddLeg("Box38", "Box27", "Box30");
}

void SpiderFootIK::LateUpdate()
{
	if (!model) return;

	UpdateModelTransform();

	for (FootIK* footIK : footIKs)
	{
		if (!footIK || !footIK->IsIKEnabled()) continue;

		footIK->UpdateGroundTarget(rayUp, rayDown, contactOffset);
		footIK->SolveIK(model->GetWorldTransform());
	}
}

void SpiderFootIK::DrawGUI()
{
	ImGui::DragFloat("Model Visual Offset Y", &modelVisualOffsetY, 0.01f, -2.0f, 2.0f);
	if (waistNodeIndex >= 0)
	{
		const Vector3 waistPosition =
			model->GetNodes()[waistNodeIndex].worldTransform.Translation();
		ImGui::Text(
			"Waist Dummy02: %.3f, %.3f, %.3f",
			waistPosition.x,
			waistPosition.y,
			waistPosition.z);
	}
	ImGui::DragFloat("Ray Up", &rayUp, 0.01f, 0.0f, 5.0f);
	ImGui::DragFloat("Ray Down", &rayDown, 0.01f, 0.0f, 10.0f);
	ImGui::DragFloat("Contact Offset", &contactOffset, 0.001f, 0.0f, 0.2f);
	if (ImGui::Checkbox("Show Foot Debug", &showFootDebug))
	{
		ApplyFootSettings();
	}
	if (ImGui::DragFloat("Root Correction X", &rootCorrectionXDeg, 1.0f, -180.0f, 180.0f))
	{
		ApplyRootRotationOffset();
	}
}

void SpiderFootIK::AddLeg(const char* rootName, const char* midName, const char* tipName, const char* contactName)
{
	if (!owner) return;
	if (!model) return;

	FootIK* footIK = owner->AddComponent<FootIK>(
		layerId,
		model,
		rootName,
		midName,
		tipName,
		contactName);

	footIK->SetPoleLiftY(0.0f);
	footIKs.push_back(footIK);
	ApplyFootSettings();
	ApplyRootRotationOffset();
}

void SpiderFootIK::SetRay(float up, float down, float contactOffset)
{
	rayUp = up;
	rayDown = down;
	this->contactOffset = contactOffset;
}

void SpiderFootIK::UpdateModelTransform()
{
	Actor* actor = GetOwnerAsActor();
	if (!actor) return;

	model->UpdateTransform(
		actor->transform.matrix *
		Matrix::CreateTranslation(0.0f, modelVisualOffsetY, 0.0f));
}

void SpiderFootIK::ApplyRootRotationOffset()
{
	const Quaternion offset =
		Quaternion::CreateFromYawPitchRoll(0.0f, RAD(rootCorrectionXDeg), 0.0f);

	for (FootIK* footIK : footIKs)
	{
		if (footIK) footIK->SetRootRotationOffset(offset);
	}
}

void SpiderFootIK::ApplyFootSettings()
{
	for (FootIK* footIK : footIKs)
	{
		if (!footIK) continue;

		footIK->SetLiftOnly(true);
		footIK->SetAlwaysRenderDebug(showFootDebug);
	}
}
