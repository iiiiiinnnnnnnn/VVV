#include "VmdlComponentEditor.h"
#include "Gameplay/Scene/VmdlEditorScene.h"
#include "Application/SettingsAndDebug/PhysicsLayerManager.h"

bool VmdlColliderEditor::DrawGUI(VmdlEditorScene& scene, int nodeIndex, bool openSelectedComponent)
{
	using AttachedComponentType = VmdlEditorScene::AttachedComponentType;
	bool duplicatePending = false;
	auto& data = scene.model->GetVmdlExtensionData();
	// コライダー
	int deleteCollider = -1;
	for (int i = 0; i < Count(data.colliders); ++i)
	{
		auto& value = data.colliders[i];
		if (value.nodeIndex != nodeIndex || scene.selectedComponentType != AttachedComponentType::Collider ||
			scene.selectedComponentIndex != i) continue;
		ImGui::PushID(2000 + i);
		const bool selected = scene.selectedComponentType == AttachedComponentType::Collider &&
			scene.selectedComponentIndex == i;
		if (selected && openSelectedComponent) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
		if (ImGuiToolKit::TreeNodeEx(ICON_FA_SHAPES " " u8"コライダー",
				selected ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None))
		{
			if (selected && openSelectedComponent) ImGui::SetScrollHereY(0.25f);
			const char* shapes[] = {JText(u8"ボックス"), JText(u8"球"),
				JText(u8"カプセル"), "Mesh"};
			bool initialActive = scene.model->GetColliderInitialActive(i);
			bool changed = ImGuiToolKit::InputText(u8"名前", &value.name);
			if (changed) value.name = ToUpperString(value.name);
			PhysicsLayerManager& layerManager = PhysicsLayerManager::Instance();
			const std::string layerPreview =
				value.layer < 0
					? JText(u8"-1: 継承")
					: layerManager.GetLayerDisplayName(static_cast<LayerId>(value.layer));
			if (ImGuiToolKit::BeginCombo(u8"レイヤー", layerPreview.c_str()))
			{
				if (ImGuiToolKit::Selectable(u8"-1: 継承", value.layer < 0))
				{
					value.layer = -1;
					changed = true;
				}
				for (int layer = 0; layer < EditableLayerCount; ++layer)
				{
					const std::string label =
						layerManager.GetLayerDisplayName(static_cast<LayerId>(layer));
					if (ImGui::Selectable(label.c_str(), value.layer == layer))
					{
						value.layer = layer;
						changed = true;
					}
				}
				ImGui::EndCombo();
			}
			int shape = value.shape >= 3 ? 3 : value.shape;
			if (ImGuiToolKit::Combo(u8"形状", &shape, shapes, IM_ARRAYSIZE(shapes)))
			{
				value.shape = shape == 3 ? 3 : shape;
				if (shape == 1) value.size.y = value.size.z = value.size.x;
				changed = true;
			}
			Vector3 scaledSize = scene.model->GetScaledAttachmentVector(value.size);
			changed |= DrawTransform(*scene.model, value.transform);
			if (shape == 3)
			{
				auto& meshes = scene.model->GetMeshes();
				int meshIndex = value.shape - 3;
				if (meshes.empty())
				{
					ImGuiToolKit::TextDisabled(u8"参照できるメッシュがありません");
				}
				else
				{
					meshIndex = std::clamp(meshIndex, 0, Count(meshes) - 1);
					if (value.shape != meshIndex + 3)
					{
						value.shape = meshIndex + 3;
						changed = true;
					}
					const std::string preview = MakeMeshLabel(*scene.model, meshIndex);
					if (ImGuiToolKit::BeginCombo(u8"参照メッシュ", preview.c_str()))
					{
						for (int candidate = 0; candidate < Count(meshes); ++candidate)
						{
							const std::string label = MakeMeshLabel(*scene.model, candidate);
							if (ImGui::Selectable(label.c_str(), candidate == meshIndex))
							{
								meshIndex = candidate;
								value.shape = candidate + 3;
								changed = true;
							}
						}
						ImGui::EndCombo();
					}
				}
			}
			else if (shape == 1)
			{
				if (ImGuiToolKit::DragSizeFloat(u8"半径", &scaledSize.x, 0.01f, 0.001f))
				{
					scaledSize.x = std::max(0.001f, scaledSize.x);
					scaledSize.y = scaledSize.z = scaledSize.x;
					value.size = scene.model->GetUnscaledAttachmentVector(scaledSize);
					changed = true;
				}
			}
			else if (value.shape == 2)
			{
				bool sizeChanged =
					ImGuiToolKit::DragSizeFloat(u8"半径", &scaledSize.x, 0.01f, 0.001f);
				sizeChanged |=
					ImGuiToolKit::DragSizeFloat(u8"高さ", &scaledSize.y, 0.01f, 0.001f);
				if (sizeChanged)
				{
					scaledSize.x = std::max(0.001f, scaledSize.x);
					scaledSize.y = std::max(0.001f, scaledSize.y);
					value.size = scene.model->GetUnscaledAttachmentVector(scaledSize);
					changed = true;
				}
			}
			else if (ImGuiToolKit::DragFloat3(u8"大きさ", &scaledSize.x, 0.01f))
			{
				value.size = scene.model->GetUnscaledAttachmentVector(scaledSize);
				changed = true;
			}
			changed |= ImGuiToolKit::Checkbox(u8"トリガー", &value.trigger);
			changed |= ImGuiToolKit::Checkbox(u8"NavMesh障害物", &value.navMeshObstacle);
			if (ImGui::IsItemHovered())
				ImGuiToolKit::SetTooltip(u8"このコライダーの範囲を、経路探索で避ける障害物にします");
			if (changed) scene.MarkDirty();
			if (ImGuiToolKit::Checkbox(u8"初期状態で有効", &initialActive))
			{
				scene.model->SetColliderInitialActive(i, initialActive);
				if (Count(scene.previewColliderActive) <= i)
					scene.previewColliderActive.resize(i + 1, 1);
				scene.previewColliderActive[i] = initialActive ? 1 : 0;
				scene.MarkDirty();
			}

			if (ImGui::Button(ICON_FA_TRASH "##DeleteComponent")) deleteCollider = i;
			if (ImGui::IsItemHovered()) ImGuiToolKit::SetTooltip(u8"削除");
			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_COPY "##DuplicateComponent")) duplicatePending = true;
			if (ImGui::IsItemHovered()) ImGuiToolKit::SetTooltip(u8"複製");
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	if (deleteCollider >= 0) scene.DeleteAttachedComponent(AttachedComponentType::Collider, deleteCollider);
	return duplicatePending;
}
