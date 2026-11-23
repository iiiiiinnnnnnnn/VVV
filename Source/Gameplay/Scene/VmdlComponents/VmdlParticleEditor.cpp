#include "VmdlComponentEditor.h"
#include "Gameplay/Scene/VmdlEditorScene.h"
#include "Rendering/Component/VMDLParticleEmitterComponent.h"

bool VmdlParticleEditor::DrawGUI(VmdlEditorScene& scene, int nodeIndex, bool openSelectedComponent)
{
	using AttachedComponentType = VmdlEditorScene::AttachedComponentType;
	bool duplicatePending = false;
	// パーティクルエミッタ
	auto& particleData = scene.model->GetVmdlParticleData();
	int deleteParticle = -1;
	for (int i = 0; i < Count(particleData.emitters); ++i)
	{
		auto& value = particleData.emitters[i];
		if (value.nodeIndex != nodeIndex || scene.selectedComponentType != AttachedComponentType::Particle ||
			scene.selectedComponentIndex != i) continue;
		ImGui::PushID(5500 + i);
		const bool selected = scene.selectedComponentType == AttachedComponentType::Particle &&
			scene.selectedComponentIndex == i;
		if (selected && openSelectedComponent) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		if (ImGui::TreeNodeEx(ICON_FA_MAGIC " " "Effekseer",
				selected ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None))
		{
			bool changed = ImGui::InputText("名前", &value.name);
			if (changed) value.name = ToUpperString(value.name);
			changed |= ImGui::Checkbox("ノードに追従", &value.modelDerived);
			changed |= ImGui::Checkbox("ビルボード", &value.billboard);
			changed |= DrawTransform(*scene.model, value.transform);
			changed |= scene.DrawEffectSelector("エフェクト", value.effectName);
			ImGui::BeginDisabled(value.effectName.empty());
			if (ImGui::Button(ICON_FA_PLAY "##TestPlay"))
			{
				scene.showParticle = true;
				scene.unifiedPreviewActive = true;
				if (!scene.particlePreviewOwner || scene.particlePreviewComponents.size() !=
					particleData.emitters.size())
					scene.RebuildParticlePreview();
				if (i < Count(scene.particlePreviewComponents) &&
					scene.particlePreviewComponents[i])
				{
					scene.manualParticlePreviewIndex = i;
					scene.particlePreviewComponents[i]->Play();
				}
			}
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				ImGui::SetTooltip("テスト再生");
			ImGui::EndDisabled();
			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_TRASH "##DeleteComponent")) deleteParticle = i;
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("削除");
			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_COPY "##DuplicateComponent")) duplicatePending = true;
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("複製");
			if (changed) { scene.MarkDirty(); scene.RebuildParticlePreview(); }
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	if (deleteParticle >= 0) scene.DeleteAttachedComponent(AttachedComponentType::Particle, deleteParticle);
	return duplicatePending;
}
