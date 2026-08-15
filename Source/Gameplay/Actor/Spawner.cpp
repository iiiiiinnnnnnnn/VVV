#include "Gameplay/Actor/Spawner.h"

#include <algorithm>

#include "Gameplay/Actor/Actor.h"
#include "Gameplay/Actor/ActorManager.h"
#include "Rendering/Component/VMDLModelComponent.h"

Spawner::Spawner(Object* owner, std::string entityName)
	: Component(owner), entityName(std::move(entityName))
{}

Actor* Spawner::Summon()
{
	if (!actorManager || !factory) return nullptr;

	std::shared_ptr<Actor> summoned = factory(summonTransform);
	if (!summoned) return nullptr;

	Actor* result = summoned.get();
	summonedActors.emplace_back(summoned);
	actorManager->Register(std::move(summoned));
	return result;
}

void Spawner::ClearSummonedActors()
{
	for (const std::weak_ptr<Actor>& reference : summonedActors)
	{
		if (std::shared_ptr<Actor> actor = reference.lock()) actor->Destroy();
	}
	summonedActors.clear();
}

void Spawner::SetEditorPreview(bool enabled)
{
	if (editorPreview == enabled) return;
	editorPreview = enabled;

	VMDLModelComponent* modelComponent = owner->GetComponent<VMDLModelComponent>();
	if (!modelComponent || !modelComponent->GetModel()) return;

	auto& materialParams = modelComponent->GetRenderParams().materials;
	materialParams.clear();
	if (!editorPreview) return;

	for (const VMDLModel::Material& material : modelComponent->GetModel()->GetMaterials())
	{
		Color color = material.baseColor;
		color.w = std::min(color.w, 0.35f);
		materialParams[material.name].baseColor = color;
	}
}

void Spawner::DrawGUI()
{
	ImGui::Text((const char*)u8"生成対象: %s", entityName.c_str());
	ImGui::Text((const char*)u8"生成数: %d", static_cast<int>(summonedActors.size()));
}
