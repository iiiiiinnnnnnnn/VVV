#include "Animation/SpringBoneCollider.h"
#include "Resource/VMDLModel.h"
#include "Rendering/Core/Graphics.h"
#include "Rendering/Renderer/ShapeRenderer.h"
#include <imgui.h>
#include <imgui_stdlib.h>
#include <ImGuiToolKit/Drag.h>

void SpringBoneCollider::ResolveCollision(Vector3& position, const Matrix& world, float pointRadius) const
{
	if (radius <= 0.0f) return;
	const Vector3 worldStart = Vector3::Transform(start, world);
	const Vector3 worldEnd = Vector3::Transform(end, world);
	const Vector3 segment = worldEnd - worldStart;
	const float lengthSquared = segment.LengthSquared();
	const float t = lengthSquared > 0.00001f
		? std::clamp((position - worldStart).Dot(segment) / lengthSquared, 0.0f, 1.0f) : 0.0f;
	const Vector3 closest = worldStart + segment * t;
	Vector3 direction = position - closest;
	const float distance = direction.Length();
	const float effectiveRadius = radius + pointRadius;
	if (distance >= effectiveRadius) return;
	direction = distance < 0.0001f ? Vector3::Up : direction / distance;
	position = closest + direction * effectiveRadius;
}

void SpringBoneCollider::Render(ShapeRenderer& renderer, const Matrix& world) const
{
	if (radius <= 0.0f) return;
	const Vector3 worldStart = Vector3::Transform(start, world);
	const Vector3 worldEnd = Vector3::Transform(end, world);
	const Vector3 segment = worldEnd - worldStart;
	const float height = segment.Length();
	const Color color(0.0f, 1.0f, 1.0f, 0.5f);
	// 両端が一致するコライダーは球として判定されるため、描画も球に揃える
	if (height <= 0.0001f)
	{
		renderer.DrawSphere(worldStart, radius, color);
		return;
	}
	const Vector3 axisY = segment / height;
	Vector3 axisX = std::abs(axisY.Dot(Vector3::Up)) > 0.99f
		? Vector3::UnitX.Cross(axisY) : Vector3::Up.Cross(axisY);
	axisX.Normalize();
	Vector3 axisZ = axisX.Cross(axisY);
	axisZ.Normalize();
	const Vector3 center = (worldStart + worldEnd) * 0.5f;
	Matrix transform = Matrix::Identity;
	transform._11 = axisX.x;
	transform._12 = axisX.y;
	transform._13 = axisX.z;
	transform._21 = axisY.x;
	transform._22 = axisY.y;
	transform._23 = axisY.z;
	transform._31 = axisZ.x;
	transform._32 = axisZ.y;
	transform._33 = axisZ.z;
	transform.Translation(center);
	renderer.DrawCapsule(transform, radius, height, color);
}

bool SpringBoneCollider::DrawGUI(const VMDLModel& model)
{
	const auto& nodes = model.GetNodes();
	const char* currentNode = nodeIndex >= 0 && nodeIndex < static_cast<int>(nodes.size())
		? nodes[nodeIndex].name.c_str() : "None";
	ImGui::PushItemWidth(240.0f);
	bool changed = false;
	if (ImGui::BeginCombo("Node", currentNode))
	{
		if (ImGui::Selectable("None", nodeIndex < 0))
		{
			nodeIndex = -1;
			changed = true;
		}
		for (int index = 0; index < static_cast<int>(nodes.size()); ++index)
		{
			const bool selected = nodeIndex == index;
			if (ImGui::Selectable(nodes[index].name.c_str(), selected))
			{
				nodeIndex = index;
				changed = true;
			}
			if (selected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	changed |= ImGuiToolKit::DragPosFloat3("Start", &start.x, 0.01f);
	changed |= ImGuiToolKit::DragPosFloat3("End", &end.x, 0.01f);
	changed |= ImGuiToolKit::DragSizeFloat("Radius", &radius, 0.005f, 0.001f, 10.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
	ImGui::PopItemWidth();
	return changed;
}

SpringBoneCollider::SpringBoneCollider(Object* owner, VMDLModel* model,
	const VMDLModel::VmdlSpringCollider& settings)
	: Component(owner), model(model)
{
	start = end = settings.transform.position;
	radius = settings.radius * std::max({std::abs(settings.transform.scale.x),
		std::abs(settings.transform.scale.y), std::abs(settings.transform.scale.z)});
	nodeIndex = settings.nodeIndex;
}

Matrix SpringBoneCollider::GetWorldTransform() const
{
	if (!model) return Matrix::Identity;
	const auto& nodes = model->GetNodes();
	if (nodeIndex >= 0 && nodeIndex < static_cast<int>(nodes.size())) return nodes[nodeIndex].worldTransform;
	return Matrix::Identity;
}

void SpringBoneCollider::ResolveCollision(Vector3& position, float pointRadius) const
{
	if (!model || !IsActive()) return;
	ResolveCollision(position, GetWorldTransform(), pointRadius);
}

void SpringBoneCollider::Render(const RenderContext& rc)
{
	if (!showDebug || !model) return;
	Render(*rc.graphics->GetShapeRenderer(), GetWorldTransform());
}

void SpringBoneCollider::DrawGUI()
{
	if (model) DrawGUI(*model);
}
