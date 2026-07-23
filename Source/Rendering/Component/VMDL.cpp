// VMDL.cpp

#include "Rendering/Component/VMDL.h"

#include <stdexcept>

#include "Animation/Animator.h"
#include "Animation/HumanoidFootIK.h"
#include "Application/SettingsAndDebug/UserSettingsManager.h"
#include "Core/Object/Object.h"
#include "Physics/Core/PhysicsComponent.h"
#include "Rendering/Component/VMDLModelComponent.h"
#include "Resource/ResourceManager.h"

VMDL::VMDL(Object* owner, const std::string& path)
	: Component(owner), path(path)
{
	model = ResourceManager::Instance().LoadModel(path);
	if (!model) throw std::runtime_error("VMDL could not be loaded: " + path);

	renderer = owner->AddComponent<VMDLModelComponent>(model, ModelShaderId::PBR);
	renderer->SetAttachmentLayerId(GetAttachmentLayer());
	renderer->SetBuildEmbeddedTrails(false);
	renderer->BuildAttachments();

	animator = owner->AddComponent<Animator>(model);
	BuildFootIK();
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

LayerId VMDL::GetAttachmentLayer() const
{
	if (owner->CompareTag("Player")) return Layers::Get("PlayerAtk");
	if (owner->CompareTag("Enemy")) return Layers::Get("EnemyAtk");
	return 0;
}

void VMDL::BuildFootIK()
{
	const auto& settings = model->GetVmdlIKSettings();
	if (settings.type != 1) return;

	humanFootIK = owner->AddComponent<HumanoidFootIK>(
		Layers::Get("Foot"),
		model.get(),
		animator,
		nullptr,
		settings.pelvis.c_str(),
		settings.leftThigh.c_str(),
		settings.leftCalf.c_str(),
		settings.leftFoot.c_str(),
		settings.leftBall.c_str(),
		settings.rightThigh.c_str(),
		settings.rightCalf.c_str(),
		settings.rightFoot.c_str(),
		settings.rightBall.c_str());
}
