#include "Gameplay/Lighting/Light.h"
#include "imgui.h"

void Light::DrawGUI()
{
	Object::DrawGUI();
	transform.DrawGUI();
	if (ImGui::TreeNodeEx((const char*)u8"ライト設定", ImGuiTreeNodeFlags_DefaultOpen))
	{
		DrawLightSettings();
		ImGui::TreePop();
	}
}

void Light::DrawLightSettings()
{
	ImGui::ColorEdit3(
		(const char*)u8"色",
		&color.x,
		ImGuiColorEditFlags_Float);

	ImGui::DragFloat(
		(const char*)u8"強度",
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
	ImGui::TextDisabled(
		(const char*)u8"向き: %.3f, %.3f, %.3f",
		transform.forward.x,
		transform.forward.y,
		transform.forward.z);
}

void PointLight::DrawLightSettings()
{
	Light::DrawLightSettings();
	ImGui::DragFloat(
		(const char*)u8"範囲",
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
	ImGui::DragFloat(
		(const char*)u8"範囲",
		&range,
		0.1f,
		0.0f,
		10000.0f,
		"%.2f");

	ImGui::DragFloat(
		(const char*)u8"内側コーン角度",
		&innerConeAngle,
		0.001f,
		0.0f,
		1.0f,
		"%.3f");

	ImGui::DragFloat(
		(const char*)u8"外側コーン角度",
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

	ImGui::TextDisabled(
		(const char*)u8"向き: %.3f, %.3f, %.3f",
		transform.forward.x,
		transform.forward.y,
		transform.forward.z);
}

void AreaLight::DrawLightSettings()
{
	Light::DrawLightSettings();
	ImGui::DragFloat(
		(const char*)u8"幅",
		&width,
		0.01f,
		0.0f,
		10000.0f,
		"%.2f");

	ImGui::DragFloat(
		(const char*)u8"高さ",
		&height,
		0.01f,
		0.0f,
		10000.0f,
		"%.2f");

	ImGui::DragFloat(
		(const char*)u8"範囲",
		&range,
		0.1f,
		0.0f,
		10000.0f,
		"%.2f");

	width = std::max(width, 0.0f);

	height = std::max(height, 0.0f);

	range = std::max(range, 0.0f);

	ImGui::TextDisabled(
		(const char*)u8"向き: %.3f, %.3f, %.3f",
		transform.forward.x,
		transform.forward.y,
		transform.forward.z);

	ImGui::TextDisabled(
		(const char*)u8"右方向: %.3f, %.3f, %.3f",
		transform.right.x,
		transform.right.y,
		transform.right.z);
}
