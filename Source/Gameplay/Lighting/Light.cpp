#include "Gameplay/Lighting/Light.h"
#include <ImGuiToolKit/Widgets.h>
#include <ImGuiToolKit/Drag.h>
#include <ImGuiToolKit/Color.h>
#include "imgui.h"

void Light::DrawGUI()
{
	DrawBasicInspector();
	ImGui::SetNextItemOpen(true, ImGuiCond_Once);
	transform.DrawGUI();
	if (ImGuiToolKit::TreeNodeEx(u8"ライト設定", ImGuiTreeNodeFlags_DefaultOpen))
	{
		DrawLightSettings();
		ImGui::TreePop();
	}
}

void Light::DrawLightSettings()
{
	ImGuiToolKit::ColorEdit3(u8"色",
		&color.x,
		ImGuiColorEditFlags_Float);

	ImGuiToolKit::DragFloat(u8"強度",
		&intensity,
		0.1f,
		0.0f,
		100000.0f,
		"%.2f");

	intensity = std::max(intensity, 0.0f);
}

void DirectionalLight::DrawLightSettings()
{
	Light::DrawLightSettings();
	ImGuiToolKit::TextDisabled(u8"向き: %.3f, %.3f, %.3f",
		transform.GetForward().x,
		transform.GetForward().y,
		transform.GetForward().z);
}

void PointLight::DrawLightSettings()
{
	Light::DrawLightSettings();
	ImGuiToolKit::DragFloat(u8"範囲",
		&range,
		0.1f,
		0.0f,
		10000.0f,
		"%.2f");

	range = std::max(range, 0.0f);
}

void SpotLight::DrawLightSettings()
{
	Light::DrawLightSettings();
	ImGuiToolKit::DragFloat(u8"範囲",
		&range,
		0.1f,
		0.0f,
		10000.0f,
		"%.2f");

	ImGuiToolKit::DragRotFloat(u8"内側コーン角度",
		&innerConeAngle,
		0.001f,
		0.0f,
		1.0f,
		"%.3f");

	ImGuiToolKit::DragRotFloat(u8"外側コーン角度",
		&outerConeAngle,
		0.001f,
		0.0f,
		1.0f,
		"%.3f");

	range = std::max(range, 0.0f);

	innerConeAngle =
		std::clamp(
		innerConeAngle,
		0.0f,
		1.0f);

	outerConeAngle =
		std::clamp(
		outerConeAngle,
		0.0f,
		1.0f);

	if (innerConeAngle < outerConeAngle)
	{
		innerConeAngle =
			outerConeAngle;
	}

	ImGuiToolKit::TextDisabled(u8"向き: %.3f, %.3f, %.3f",
		transform.GetForward().x,
		transform.GetForward().y,
		transform.GetForward().z);
}

void AreaLight::DrawLightSettings()
{
	Light::DrawLightSettings();
	ImGuiToolKit::DragFloat(u8"幅",
		&width,
		0.01f,
		0.0f,
		10000.0f,
		"%.2f");

	ImGuiToolKit::DragSizeFloat(u8"高さ",
		&height,
		0.01f,
		0.0f,
		10000.0f,
		"%.2f");

	ImGuiToolKit::DragFloat(u8"範囲",
		&range,
		0.1f,
		0.0f,
		10000.0f,
		"%.2f");

	width = std::max(width, 0.0f);

	height = std::max(height, 0.0f);

	range = std::max(range, 0.0f);

	ImGuiToolKit::TextDisabled(u8"向き: %.3f, %.3f, %.3f",
		transform.GetForward().x,
		transform.GetForward().y,
		transform.GetForward().z);

	ImGuiToolKit::TextDisabled(u8"右方向: %.3f, %.3f, %.3f",
		transform.GetRight().x,
		transform.GetRight().y,
		transform.GetRight().z);
}
