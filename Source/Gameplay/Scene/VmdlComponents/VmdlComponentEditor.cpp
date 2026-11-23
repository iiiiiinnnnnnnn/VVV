#include "VmdlComponentEditor.h"

std::string VmdlComponentEditor::MakeMeshLabel(const VMDLModel& model, int meshIndex)
{
	const auto& meshes = model.GetMeshes();
	if (meshIndex < 0 || meshIndex >= Count(meshes)) return "メッシュなし";

	const auto& mesh = meshes[meshIndex];
	const std::string nodeName = mesh.node ? mesh.node->name : "ノードなし";
	const std::string materialName = mesh.material ? mesh.material->name : "マテリアルなし";
	return std::format("{}: {} / {}", meshIndex, nodeName, materialName);
}

bool VmdlComponentEditor::DrawTransform(VMDLModel& model, VMDLModel::VmdlComponentTransform& transform)
{
	ImGui::SeparatorText("Transform Offset");
	bool changed = false;
	Vector3 position = model.GetScaledAttachmentVector(transform.position);
	if (ImGuiToolKit::DragPosFloat3("位置", &position.x, 0.01f))
	{
		transform.position = model.GetUnscaledAttachmentVector(position);
		changed = true;
	}
	changed |= ImGuiToolKit::DragRotFloat3("回転", &transform.rotation.x, 0.1f);
	changed |= ImGuiToolKit::DragSizeFloat3("スケール", &transform.scale.x, 0.01f);
	return changed;
}

void VmdlComponentEditor::DrawDistanceMode(bool& attenuate)
{
	bool fixed = !attenuate;
	if (ImGui::Checkbox("範囲内は固定強度", &fixed) && fixed)
		attenuate = false;
	bool distance = attenuate;
	if (ImGui::Checkbox("プレイヤーとの距離に応じて減衰", &distance) && distance)
		attenuate = true;
}
