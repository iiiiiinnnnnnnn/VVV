#include "VmdlComponentEditor.h"
#include "Gameplay/Scene/VmdlEditorScene.h"

bool VmdlRadialBlurEditor::DrawGUI(VmdlEditorScene& scene, int nodeIndex, bool openSelectedComponent)
{
	using AttachedComponentType = VmdlEditorScene::AttachedComponentType;
	bool duplicatePending = false;
	auto& presentation = scene.model->GetVmdlPresentationData();
	int deleteRadialBlur = -1;
	for (int i = 0; i < Count(presentation.radialBlurs); ++i)
	{
		auto& value = presentation.radialBlurs[i];
		if (value.nodeIndex != nodeIndex || scene.selectedComponentType != AttachedComponentType::RadialBlur ||
			scene.selectedComponentIndex != i) continue;
		ImGui::PushID(8000 + i);
		const bool selected = scene.selectedComponentType == AttachedComponentType::RadialBlur && scene.selectedComponentIndex == i;
		if (selected && openSelectedComponent) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		if (ImGuiToolKit::TreeNodeEx(ICON_FA_SUN " " u8"ラジアルブラー", selected ? ImGuiTreeNodeFlags_Selected : 0))
		{
			bool changed = ImGuiToolKit::InputText(u8"名前", &value.name);
			if (changed) value.name = ToUpperString(value.name);
			changed |= DrawTransform(*scene.model, value.transform);
			changed |= ImGuiToolKit::DragFloat(u8"効果範囲", &value.range, 0.1f, 0.01f, 10000.0f);
			const bool oldMode = value.distanceAttenuation;
			DrawDistanceMode(value.distanceAttenuation);
			changed |= oldMode != value.distanceAttenuation;
			changed |= ImGuiToolKit::DragDelayFloat(u8"継続時間", &value.duration, 0.05f, 0.01f, 30.0f);
			changed |= ImGuiToolKit::DragFloat(u8"ブラー強度", &value.power, 0.05f, 0.0f, 20.0f);
			changed |= ImGuiToolKit::SliderFloat(u8"立ち上がり割合", &value.attackRate, 0.01f, 0.95f);
			value.range = std::max(0.01f, value.range);
			value.duration = std::max(0.01f, value.duration);
			value.power = std::max(0.0f, value.power);
			if (changed) scene.MarkDirty();
			if (ImGui::Button(ICON_FA_TRASH "##DeleteComponent")) deleteRadialBlur = i;
			if (ImGui::IsItemHovered()) ImGuiToolKit::SetTooltip(u8"削除");
			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_COPY "##DuplicateComponent")) duplicatePending = true;
			if (ImGui::IsItemHovered()) ImGuiToolKit::SetTooltip(u8"複製");
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	if (deleteRadialBlur >= 0) scene.DeleteAttachedComponent(AttachedComponentType::RadialBlur, deleteRadialBlur);
	return duplicatePending;
}
