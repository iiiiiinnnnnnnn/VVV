// VMDL.cpp

#include "Rendering/Component/VMDL.h"

#include <stdexcept>

#include "Animation/Animator.h"
#include "Animation/HumanoidFootIK.h"
#include "Animation/SpiderFootIK.h"
#include "Application/SettingsAndDebug/PhysicsLayerManager.h"
#include "Core/Object/Object.h"
#include "Physics/Core/PhysicsComponent.h"
#include "Rendering/Component/VMDLModelComponent.h"
#include "Resource/ResourceManager.h"

VMDL::VMDL(Object* owner, const std::string& path)
	: Component(owner), path(path)
{
	auto loaded = ResourceManager::Instance().LoadModel(path);
	if (!loaded) throw std::runtime_error("VMDL could not be loaded: " + path);
	model = loaded->Clone();

	renderer = owner->AddComponent<VMDLModelComponent>(model, ModelShaderId::VMat);
	renderer->SetAttachmentLayerId(GetAttachmentLayer());
	renderer->BuildAttachments();

	animator = owner->AddComponent<Animator>(model);
	BuildFootIK();

	ApplyMorph("DEFAULT");
}

void VMDL::OnDrawGUI()
{
	ImGui::TextWrapped("Path: %s", path.c_str());
	ImGui::Text("Model: %s", model ? "Loaded" : "Not loaded");
}

PhysicsComponent* VMDL::GetCollider(const std::string& name) const
{
	return renderer ? renderer->GetAttachmentCollider(name) : nullptr;
}

void VMDL::SetAutoUpdateTransform(bool value)
{
	if (renderer) renderer->SetAutoUpdateTransform(value);
}

bool VMDL::ApplyMorph(const std::string& morphName)
{
	return model && model->ApplyMorph(morphName.c_str());
}

LayerId VMDL::GetAttachmentLayer() const
{
	if (owner->CompareTag("Player")) return Layers::Get("PlayerAtk");
	if (owner->CompareTag("Enemy")) return Layers::Get("EnemyAtk");
	return 0;
}

void VMDL::BuildFootIK()
{
	model->EnsureVmdlIKSettingsCompatibility();
	const auto& settings = model->GetVmdlIKSettings();
	if (settings.type == 0) return;

	const auto isValidLeg = [this](const VMDLModel::VmdlIKLeg& leg)
	{
		return model->GetNodeIndex(leg.root.c_str()) >= 0 &&
			model->GetNodeIndex(leg.mid.c_str()) >= 0 &&
			model->GetNodeIndex(leg.tip.c_str()) >= 0 &&
			(leg.contact.empty() || model->GetNodeIndex(leg.contact.c_str()) >= 0);
	};
	if (settings.type == 1)
	{
		if (settings.legs.size() < 2 || !isValidLeg(settings.legs[0]) || !isValidLeg(settings.legs[1])) return;
		const auto& left = settings.legs[0];
		const auto& right = settings.legs[1];
		humanFootIK = owner->AddComponent<HumanoidFootIK>(
			Layers::Get("Foot"), model.get(), animator, nullptr, settings.centerNode.c_str(),
			left.root.c_str(), left.mid.c_str(), left.tip.c_str(), left.contact.empty() ? nullptr : left.contact.c_str(),
			right.root.c_str(), right.mid.c_str(), right.tip.c_str(), right.contact.empty() ? nullptr : right.contact.c_str());
		return;
	}

	multiLegFootIK = owner->AddComponent<SpiderFootIK>(Layers::Get("Foot"), model.get(), animator);
	multiLegFootIK->SetModelVisualOffsetY(0.0f);
	multiLegFootIK->AddLegsFromVmdlSettings();
}
