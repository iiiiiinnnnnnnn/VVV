// SpiderFootIK.cpp

#include "Animation/SpiderFootIK.h"

#include "Gameplay/Actor/Actor.h"
#include "Animation/Animator.h"
#include "Core/Object/Object.h"

SpiderFootIK::SpiderFootIK(Object* owner, LayerId layerId, VMDLModel* model, Animator* animator)
	: Component(owner), model(model), animator(animator), layerId(layerId)
{

}

void SpiderFootIK::LateUpdate()
{
	if (!model) return;

	UpdateModelTransform();

	for (int footIndex = 0; footIndex < static_cast<int>(footIKs.size()); ++footIndex)
	{
		FootIK* footIK = footIKs[footIndex];
		if (!footIK || !footIK->IsIKEnabled()) continue;

		const float weight = GetFootIKWeight(footIndex);
		footIK->SetWeight(weight);
		footIK->SetDownwardWeight(weight);
		footIK->SetMaxDownCorrection(maxDownCorrection);
		footIK->UpdateGroundTarget(rayUp, rayDown, contactOffset);
		footIK->SolveIK(model->GetWorldTransform());
	}
}

void SpiderFootIK::DrawGUI()
{
	ImGui::DragFloat("VMDLModel Visual Offset Y", &modelVisualOffsetY, 0.01f, -2.0f, 2.0f);
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
	if (!rootName || !midName || !tipName) return;
	if (model->GetNodeIndex(rootName) < 0 || model->GetNodeIndex(midName) < 0 || model->GetNodeIndex(tipName) < 0) return;
	if (contactName && model->GetNodeIndex(contactName) < 0) return;

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

int SpiderFootIK::AddLegsFromVmdlSettings()
{
	if (!model) return 0;
	model->EnsureVmdlIKSettingsCompatibility();
	const auto& settings = model->GetVmdlIKSettings();
	if (settings.type != 2 && settings.type != 3) return 0;

	waistNodeIndex = model->GetNodeIndex(settings.centerNode.c_str());
	int addedCount = 0;
	for (const auto& leg : settings.legs)
	{
		const size_t previousCount = footIKs.size();
		AddLeg(
			leg.root.c_str(), leg.mid.c_str(), leg.tip.c_str(),
			leg.contact.empty() ? nullptr : leg.contact.c_str());
		if (footIKs.size() > previousCount) ++addedCount;
	}
	return addedCount;
}

void SpiderFootIK::SetRay(float up, float down, float contactOffset)
{
	rayUp = up;
	rayDown = down;
	this->contactOffset = contactOffset;
}

bool SpiderFootIK::HasGroundContact() const
{
	for (const FootIK* footIK : footIKs)
	{
		if (!footIK) continue;
		if (!footIK->IsIKEnabled()) continue;
		if (footIK->HasGroundContact()) return true;
	}
	return false;
}

void SpiderFootIK::UpdateModelTransform()
{
	Actor* actor = dynamic_cast<Actor*>(owner);
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
	if (model)
	{
		const int ikType = model->GetVmdlIKSettings().type;
		if (ikType == 2 || ikType == 3)
		{
			return model->EvaluateFootIKWeight(
				animator->GetCurrentAnimationIndex(),
				animator->GetCurrentAnimationTime(),
				footIndex);
		}
	}

	return 0.0f;
}
