#include "VmdlComponentEditor.h"
#include "Gameplay/Scene/VmdlEditorScene.h"

bool VmdlSpringEditor::DrawGUI(VmdlEditorScene& scene, int nodeIndex, bool openSelectedComponent)
{
	using AttachedComponentType = VmdlEditorScene::AttachedComponentType;
	bool duplicatePending = false;
	auto& data = scene.model->GetVmdlExtensionData();
	// スプリング
	int deleteSpring = -1;
	for (int i = 0; i < Count(data.springs); ++i)
	{
		auto& value = data.springs[i];
		if (value.nodeIndex != nodeIndex || scene.selectedComponentType != AttachedComponentType::Spring ||
			scene.selectedComponentIndex != i) continue;
		ImGui::PushID(3000 + i);
		const bool selected = scene.selectedComponentType == AttachedComponentType::Spring &&
			scene.selectedComponentIndex == i;
		if (selected && openSelectedComponent) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		if (ImGui::TreeNodeEx(ICON_FA_LINK " " "スプリング",
				selected ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None))
		{
			if (selected && openSelectedComponent) ImGui::SetScrollHereY(0.25f);
			bool changed = ImGui::InputText("名前", &value.name);
			if (changed) value.name = ToUpperString(value.name);
			changed |= DrawTransform(*scene.model, value.transform);
			changed |= ImGui::DragFloat("硬さ", &value.stiffness, 0.01f, 0.0f, 1.0f);
			changed |= ImGui::DragFloat("抵抗", &value.drag, 0.01f, 0.0f, 1.0f);
			changed |= ImGui::SliderFloat("最大角度 (度)",
				&value.maxAngleDegrees, 0.0f, 180.0f, "%.1f");
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("アニメーションの向きを中心に揺れる範囲を制限します。0度で揺れなし、180度で制限なし。コライダーと両立できない場合は角度制限を優先します。");
			bool angleChanged = ImGui::SliderFloat3("最小角度 XYZ (度)",
				&value.minAnglesDegrees.x, -180.0f, 180.0f, "%.1f");
			angleChanged |= ImGui::SliderFloat3("最大角度 XYZ (度)",
				&value.maxAnglesDegrees.x, -180.0f, 180.0f, "%.1f");
			if (angleChanged)
			{
				const Vector3 minimum = Vector3::Min(value.minAnglesDegrees, value.maxAnglesDegrees);
				value.maxAnglesDegrees = Vector3::Max(value.minAnglesDegrees, value.maxAnglesDegrees);
				value.minAnglesDegrees = minimum;
			}
			changed |= angleChanged;
			if (changed) scene.MarkDirty();

			if (ImGui::Button(ICON_FA_TRASH "##DeleteComponent")) deleteSpring = i;
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("削除");
			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_COPY "##DuplicateComponent")) duplicatePending = true;
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("複製");
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	if (deleteSpring >= 0) scene.DeleteAttachedComponent(AttachedComponentType::Spring, deleteSpring);
	return duplicatePending;
}
