#include "VmdlComponentEditor.h"
#include "Gameplay/Scene/VmdlEditorScene.h"

bool VmdlSpringColliderEditor::DrawGUI(VmdlEditorScene& scene, int nodeIndex, bool openSelectedComponent)
{
	using AttachedComponentType = VmdlEditorScene::AttachedComponentType;
	bool duplicatePending = false;
	auto& data = scene.model->GetVmdlExtensionData();
	// スプリングコライダー
	int deleteSpringCollider = -1;
	for (int i = 0; i < Count(data.springColliders); ++i)
	{
		auto& value = data.springColliders[i];
		if (value.nodeIndex != nodeIndex ||
			scene.selectedComponentType != AttachedComponentType::SpringCollider ||
			scene.selectedComponentIndex != i) continue;
		ImGui::PushID(4000 + i);
		const bool selected = scene.selectedComponentType == AttachedComponentType::SpringCollider &&
			scene.selectedComponentIndex == i;
		if (selected && openSelectedComponent) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		if (ImGuiToolKit::TreeNodeEx(ICON_FA_BULLSEYE " " u8"スプリングコライダー",
				selected ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None))
		{
			if (selected && openSelectedComponent) ImGui::SetScrollHereY(0.25f);
			bool changed = ImGuiToolKit::InputText(u8"名前", &value.name);
			if (changed) value.name = ToUpperString(value.name);
			changed |= DrawTransform(*scene.model, value.transform);
			changed |= ImGuiToolKit::DragSizeFloat(u8"半径", &value.radius, 0.01f, 0.001f);
			if (changed) scene.MarkDirty();

			if (ImGui::Button(ICON_FA_TRASH "##DeleteComponent")) deleteSpringCollider = i;
			if (ImGui::IsItemHovered()) ImGuiToolKit::SetTooltip(u8"削除");
			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_COPY "##DuplicateComponent")) duplicatePending = true;
			if (ImGui::IsItemHovered()) ImGuiToolKit::SetTooltip(u8"複製");
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	if (deleteSpringCollider >= 0) scene.DeleteAttachedComponent(AttachedComponentType::SpringCollider, deleteSpringCollider);
	return duplicatePending;
}
