#include "VmdlComponentEditor.h"
#include "Gameplay/Scene/VmdlEditorScene.h"

bool VmdlSoundSourceEditor::DrawGUI(VmdlEditorScene& scene, int nodeIndex, bool openSelectedComponent)
{
	using AttachedComponentType = VmdlEditorScene::AttachedComponentType;
	bool duplicatePending = false;
	// サウンドソース
	auto& soundData = scene.model->GetVmdlSoundData();
	int deleteSoundSource = -1;
	for (int i = 0; i < Count(soundData.sources); ++i)
	{
		auto& value = soundData.sources[i];
		if (value.nodeIndex != nodeIndex ||
			scene.selectedComponentType != AttachedComponentType::SoundSource ||
			scene.selectedComponentIndex != i) continue;
		ImGui::PushID(6000 + i);
		const bool selected = scene.selectedComponentType == AttachedComponentType::SoundSource &&
			scene.selectedComponentIndex == i;
		if (selected && openSelectedComponent) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		if (ImGui::TreeNodeEx(ICON_FA_VOLUME_UP " " "サウンドソース",
				selected ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None))
		{
			if (selected && openSelectedComponent) ImGui::SetScrollHereY(0.25f);
			bool changed = ImGui::InputText("名前", &value.name);
			if (changed) value.name = ToUpperString(value.name);
			changed |= DrawTransform(*scene.model, value.transform);
			if (scene.DrawSoundPathSelector("サウンド", value.soundPath)) changed = true;
			bool randomVariant = value.variant < 0;
			if (ImGui::Checkbox("バリエーションをランダム選択", &randomVariant))
			{
				value.variant = randomVariant ? -1 : 0;
				changed = true;
			}
			if (!randomVariant)
				changed |= ImGui::DragInt("バリエーション番号", &value.variant, 1.0f, 0, 10000);
			changed |= ImGui::DragFloat("音量", &value.volume, 0.01f, 0.0f, 4.0f);
			changed |= ImGui::DragFloatRange2("ランダムピッチ",
				&value.pitchMin, &value.pitchMax, 0.01f, 0.125f, 8.0f, "%.2f", "%.2f");
			changed |= ImGui::Checkbox("3Dサウンド", &value.spatial);
			if (value.spatial)
			{
				changed |= ImGui::DragFloat("減衰開始距離", &value.minDistance, 0.1f, 0.01f, 1000.0f);
				changed |= ImGui::DragFloat("再生限界距離", &value.maxDistance, 0.1f, 0.02f, 1000.0f);
				changed |= ImGui::DragFloat("近距離ローパス", &value.lowPassHz, 10.0f, 20.0f, 20000.0f);
				changed |= ImGui::DragFloat("遠距離ローパス", &value.farLowPassHz, 10.0f, 20.0f, 20000.0f);
				changed |= ImGui::SliderFloat("残響", &value.reverbMix, 0.0f, 1.0f);
			}
			value.volume = std::clamp(value.volume, 0.0f, 4.0f);
			value.pitchMin = std::clamp(value.pitchMin, 0.125f, 8.0f);
			value.pitchMax = std::clamp(value.pitchMax, value.pitchMin, 8.0f);
			value.minDistance = std::max(0.01f, value.minDistance);
			value.maxDistance = std::max(value.minDistance + 0.01f, value.maxDistance);
			value.lowPassHz = std::clamp(value.lowPassHz, 20.0f, 20000.0f);
			value.farLowPassHz = std::clamp(value.farLowPassHz, 20.0f, 20000.0f);
			value.reverbMix = std::clamp(value.reverbMix, 0.0f, 1.0f);
			if (changed) scene.MarkDirty();

			ImGui::BeginDisabled(value.soundPath.empty());
			if (ImGui::Button(ICON_FA_PLAY "##TestSound")) scene.PlaySoundSourcePreview(i);
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				ImGui::SetTooltip("テスト再生");
			ImGui::EndDisabled();
			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_TRASH "##DeleteComponent")) deleteSoundSource = i;
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("削除");
			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_COPY "##DuplicateComponent")) duplicatePending = true;
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("複製");
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	if (deleteSoundSource >= 0) scene.DeleteAttachedComponent(AttachedComponentType::SoundSource, deleteSoundSource);
	return duplicatePending;
}
