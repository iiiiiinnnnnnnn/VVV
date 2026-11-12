#include "VmdlComponentEditor.h"
#include "Gameplay/Scene/VmdlEditorScene.h"

bool VmdlRigidBodyEditor::DrawGUI(VmdlEditorScene& scene, int nodeIndex, bool openSelectedComponent)
{
	using AttachedComponentType = VmdlEditorScene::AttachedComponentType;
	bool duplicatePending = false;
	auto& data = scene.model->GetVmdlExtensionData();
	// リジッドボディ
	int deleteRigidBody = -1;
	for (int i = 0; i < Count(data.rigidBodies); ++i)
	{
		auto& value = data.rigidBodies[i];
		if (value.nodeIndex != nodeIndex || scene.selectedComponentType != AttachedComponentType::RigidBody ||
			scene.selectedComponentIndex != i) continue;
		ImGui::PushID(1000 + i);
		const bool selected = scene.selectedComponentType == AttachedComponentType::RigidBody &&
			scene.selectedComponentIndex == i;
		if (selected && openSelectedComponent) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		if (ImGuiToolKit::TreeNodeEx(ICON_FA_CUBE " " u8"リジッドボディ",
				selected ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None))
		{
			if (selected && openSelectedComponent) ImGui::SetScrollHereY(0.25f);
			bool changed = ImGuiToolKit::InputText(u8"名前", &value.name);
			if (changed) value.name = ToUpperString(value.name);
			changed |= DrawTransform(*scene.model, value.transform);
			const char* rigidBodyTypes[] = {JText(u8"静的"), JText(u8"動的")};
			int rigidBodyType = value.kinematic ? 0 : 1;
			if (ImGuiToolKit::Combo(u8"種類", &rigidBodyType,
					rigidBodyTypes, IM_ARRAYSIZE(rigidBodyTypes)))
			{
				value.kinematic = rigidBodyType == 0;
				changed = true;
			}
			if (rigidBodyType == 1)
				changed |= ImGuiToolKit::DragFloat(u8"質量", &value.mass, 0.05f, 0.001f);
			if (changed) scene.MarkDirty();
			if (ImGui::Button(ICON_FA_TRASH "##DeleteComponent")) deleteRigidBody = i;
			if (ImGui::IsItemHovered()) ImGuiToolKit::SetTooltip(u8"削除");
			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_COPY "##DuplicateComponent")) duplicatePending = true;
			if (ImGui::IsItemHovered()) ImGuiToolKit::SetTooltip(u8"複製");
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	if (deleteRigidBody >= 0) scene.DeleteAttachedComponent(AttachedComponentType::RigidBody, deleteRigidBody);
	return duplicatePending;
}
