#include "VmdlComponentEditor.h"
#include "Gameplay/Scene/VmdlEditorScene.h"

bool VmdlPointLightEditor::DrawGUI(VmdlEditorScene& scene, int nodeIndex, bool openSelectedComponent)
{
	using AttachedComponentType = VmdlEditorScene::AttachedComponentType;
	bool duplicatePending = false;
	// ポイントライト
	auto& pointLights = scene.model->GetVmdlLightData().pointLights;
	int deletePointLight = -1;
	for (int i = 0; i < Count(pointLights); ++i)
	{
		auto& value = pointLights[i];
		if (value.nodeIndex != nodeIndex ||
			scene.selectedComponentType != AttachedComponentType::PointLight ||
			scene.selectedComponentIndex != i) continue;
		ImGui::PushID(6500 + i);
		const bool selected = scene.selectedComponentType == AttachedComponentType::PointLight &&
			scene.selectedComponentIndex == i;
		if (selected && openSelectedComponent) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		if (ImGuiToolKit::TreeNodeEx(ICON_FA_LIGHTBULB " " u8"ポイントライト",
				selected ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None))
		{
			if (selected && openSelectedComponent) ImGui::SetScrollHereY(0.25f);
			bool changed = ImGuiToolKit::InputText(u8"名前", &value.name);
			if (changed) value.name = ToUpperString(value.name);
			changed |= DrawTransform(*scene.model, value.transform);
			changed |= ImGuiToolKit::Checkbox(u8"有効", &value.active);
			changed |= ImGuiToolKit::ColorEdit3(u8"色", &value.color.x,
				ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
			changed |= ImGuiToolKit::DragFloat(u8"強度", &value.intensity, 0.1f, 0.0f, 100000.0f);
			changed |= ImGuiToolKit::DragFloat(u8"範囲", &value.range, 0.1f, 0.0f, 10000.0f);
			value.intensity = std::max(0.0f, value.intensity);
			value.range = std::max(0.0f, value.range);
			if (changed) scene.MarkDirty();
			if (ImGui::Button(ICON_FA_TRASH "##DeleteComponent")) deletePointLight = i;
			if (ImGui::IsItemHovered()) ImGuiToolKit::SetTooltip(u8"削除");
			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_COPY "##DuplicateComponent")) duplicatePending = true;
			if (ImGui::IsItemHovered()) ImGuiToolKit::SetTooltip(u8"複製");
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	if (deletePointLight >= 0) scene.DeleteAttachedComponent(AttachedComponentType::PointLight, deletePointLight);
	return duplicatePending;
}
