// SpiderFootIK.cpp

#include "SpiderFootIK.h"

#include "Actor.h"
#include "Animator.h"
#include "Object.h"

SpiderFootIK::SpiderFootIK(Object* owner, LayerId layerId, Model* model, Animator* animator)
	: Component(owner), model(model), animator(animator), layerId(layerId)
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

	for (int footIndex = 0; footIndex < static_cast<int>(footIKs.size()); ++footIndex)
	{
		FootIK* footIK = footIKs[footIndex];
		if (!footIK || !footIK->IsIKEnabled()) continue;

		footIK->SetDownwardWeight(GetFootIKWeight(footIndex));
		footIK->SetMaxDownCorrection(maxDownCorrection);
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
	ImGui::DragFloat("Ray Up", &rayUp, 0.01f, 0.0f);
	ImGui::DragFloat("Ray Down", &rayDown, 0.01f, 0.0f);
	ImGui::DragFloat("Contact Offset", &contactOffset, 0.001f, 0.0f);
	ImGui::DragFloat("Max Up Correction", &maxUpCorrection, 0.01f, 0.0f);
	ImGui::DragFloat("Max Down Correction", &maxDownCorrection, 0.01f, 0.0f);
	if (ImGui::Checkbox("Show Foot Debug", &showFootDebug))
	{
		ApplyFootSettings();
	}

	if (animator && model)
	{
		const int animationIndex = animator->GetCurrentAnimationIndex(0);
		const float time = animator->GetCurrentAnimationTime(0);
		ImGui::TextDisabled("FootIK Anim: %d  Time: %.3f", animationIndex, time);
		for (int footIndex = 0; footIndex < static_cast<int>(footIKs.size()); ++footIndex)
		{
			FootIK* footIK = footIKs[footIndex];
			ImGui::TextDisabled(
				"Foot %d Weight: %.2f  Hit: %s  OffsetY: %.3f  HitLayer: %d  RawLayer: %d  NormalY: %.2f",
				footIndex,
				GetFootIKWeight(footIndex),
				footIK && footIK->HasGroundContact() ? "Yes" : "No",
				footIK ? footIK->GetGroundOffsetY() : 0.0f,
				footIK ? static_cast<int>(footIK->GetLastHitLayerId()) : -1,
				footIK ? static_cast<int>(footIK->GetLastRawHitLayerId()) : -1,
				footIK ? footIK->GetLastHitNormalY() : 0.0f);
		}
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
	std::vector<std::string> targetNames;
	if (rootName && rootName[0] != '\0') targetNames.emplace_back(rootName);
	if (midName && midName[0] != '\0') targetNames.emplace_back(midName);
	if (tipName && tipName[0] != '\0') targetNames.emplace_back(tipName);
	if (contactName && contactName[0] != '\0') targetNames.emplace_back(contactName);
	footTargetNames.push_back(targetNames);
	ApplyFootSettings();
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

void SpiderFootIK::ApplyFootSettings()
{
	for (FootIK* footIK : footIKs)
	{
		if (!footIK) continue;

		footIK->SetLiftOnly(false);
		footIK->SetMaxUpCorrection(maxUpCorrection);
	}
}

float SpiderFootIK::GetFootIKWeight(int footIndex) const
{
	if (!animator) return 0.0f;
	if (footIndex < 0 || footIndex >= static_cast<int>(footTargetNames.size())) return 0.0f;

	float result = animator->EvaluateCurrentFootIKWeight("All");
	for (const std::string& targetName : footTargetNames[footIndex])
	{
		const float weight = animator->EvaluateCurrentFootIKWeight(targetName);
		if (result < weight) result = weight;
	}
	return result;
}
