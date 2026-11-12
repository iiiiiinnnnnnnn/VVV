#include "VmdlComponentEditor.h"
#include "Gameplay/Scene/VmdlEditorScene.h"

bool VmdlTrailEditor::DrawGUI(VmdlEditorScene& scene, int nodeIndex, bool openSelectedComponent)
{
	using AttachedComponentType = VmdlEditorScene::AttachedComponentType;
	bool duplicatePending = false;
	// トレイル
	auto& trailData = scene.model->GetVmdlTrailData();
	int deleteTrail = -1;
	for (int i = 0; i < Count(trailData.trails); ++i)
	{
		auto& value = trailData.trails[i];
		if (value.nodeIndex != nodeIndex || scene.selectedComponentType != AttachedComponentType::Trail ||
			scene.selectedComponentIndex != i) continue;
		ImGui::PushID(5000 + i);
		const bool selected = scene.selectedComponentType == AttachedComponentType::Trail &&
			scene.selectedComponentIndex == i;
		if (selected && openSelectedComponent) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		if (ImGuiToolKit::TreeNodeEx(ICON_FA_WIND " " u8"トレイル",
				selected ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None))
		{
			if (selected && openSelectedComponent) ImGui::SetScrollHereY(0.25f);
			bool initialActive = scene.model->GetTrailInitialActive(i);
			bool changed = ImGuiToolKit::InputText(u8"名前", &value.name);
			if (changed) value.name = ToUpperString(value.name);
			changed |= DrawTransform(*scene.model, value.transform);
			changed |=
				ImGuiToolKit::DragPosFloat3(u8"根元オフセット", &value.rootOffset.x, 0.01f);
			changed |=
				ImGuiToolKit::DragPosFloat3(u8"先端オフセット", &value.tipOffset.x, 0.01f);
			changed |= ImGuiToolKit::ColorEdit4(u8"色", &value.color.x,
				ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
			changed |=
				ImGuiToolKit::DragFloat(u8"先端比率", &value.tipRatio, 0.01f, 0.0f, 4.0f);
			changed |= ImGuiToolKit::DragDelayFloat(u8"生存時間", &value.lifeTime, 0.01f, 0.01f, 10.0f, "%.3f sec");
			changed |= ImGuiToolKit::DragInt(u8"最大頂点数", &value.maxPoints, 1.0f, 2, 1024);
			changed |=
				ImGuiToolKit::DragRotFloat3(u8"角度オフセット", &value.offsetAngle.x, 0.01f);
			value.tipRatio = std::clamp(value.tipRatio, 0.0f, 4.0f);
			value.lifeTime = std::clamp(value.lifeTime, 0.01f, 10.0f);
			value.maxPoints = std::clamp(value.maxPoints, 2, 1024);
			if (changed) scene.MarkDirty();
			if (ImGuiToolKit::Checkbox(u8"初期状態で有効", &initialActive))
			{
				scene.model->SetTrailInitialActive(i, initialActive);
				if (Count(scene.previewTrailActive) <= i)
					scene.previewTrailActive.resize(i + 1, 1);
				scene.previewTrailActive[i] = initialActive ? 1 : 0;
				scene.MarkDirty();
			}
			if (ImGui::Button(ICON_FA_TRASH "##DeleteComponent")) deleteTrail = i;
			if (ImGui::IsItemHovered()) ImGuiToolKit::SetTooltip(u8"削除");
			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_COPY "##DuplicateComponent")) duplicatePending = true;
			if (ImGui::IsItemHovered()) ImGuiToolKit::SetTooltip(u8"複製");
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	if (deleteTrail >= 0) scene.DeleteAttachedComponent(AttachedComponentType::Trail, deleteTrail);
	return duplicatePending;
}
