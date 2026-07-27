// Light.cpp

#include "Gameplay/Lighting/Light.h"
#include "imgui.h"

void Light::DrawGUI()
{
	Object::DrawGUI();

	transform.DrawGUI();

	if (ImGui::TreeNode("Light Info"))
	{
		ImGui::ColorEdit3(
			"Color",
			&color.x,
			ImGuiColorEditFlags_Float);

		ImGui::DragFloat(
			"Intensity",
			&intensity,
			0.1f,
			0.0f,
			100000.0f,
			"%.2f");

		intensity = std::max(intensity, 0.0f);

		ImGui::TreePop();
	}
}

void DirectionalLight::DrawGUI()
{
	Light::DrawGUI();

	if (ImGui::TreeNode(name.c_str()))
	{
		ImGui::TextDisabled(
			"Direction: %.3f, %.3f, %.3f",
			transform.forward.x,
			transform.forward.y,
			transform.forward.z);

		ImGui::TreePop();
	}
}

void PointLight::DrawGUI()
{
	Light::DrawGUI();

	if (ImGui::TreeNode(name.c_str()))
	{
		ImGui::DragFloat(
			"Range",
			&range,
			0.1f,
			0.0f,
			10000.0f,
			"%.2f");

		if (range < 0.0f)
		{
			range = 0.0f;
		}

		ImGui::TreePop();
	}
}

void SpotLight::DrawGUI()
{
	Light::DrawGUI();

	if (ImGui::TreeNode(name.c_str()))
	{
		ImGui::DragFloat(
			"Range",
			&range,
			0.1f,
			0.0f,
			10000.0f,
			"%.2f");

		ImGui::DragFloat(
			"Inner Cone Angle",
			&innerConeAngle,
			0.001f,
			0.0f,
			1.0f,
			"%.3f");

		ImGui::DragFloat(
			"Outer Cone Angle",
			&outerConeAngle,
			0.001f,
			0.0f,
			1.0f,
			"%.3f");

		if (range < 0.0f)
		{
			range = 0.0f;
		}

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
			"Direction: %.3f, %.3f, %.3f",
			transform.forward.x,
			transform.forward.y,
			transform.forward.z);

		ImGui::TreePop();
	}
}

void AreaLight::DrawGUI()
{
	Light::DrawGUI();

	if (ImGui::TreeNode(name.c_str()))
	{
		ImGui::DragFloat(
			"Width",
			&width,
			0.01f,
			0.0f,
			10000.0f,
			"%.2f");

		ImGui::DragFloat(
			"Height",
			&height,
			0.01f,
			0.0f,
			10000.0f,
			"%.2f");

		ImGui::DragFloat(
			"Range",
			&range,
			0.1f,
			0.0f,
			10000.0f,
			"%.2f");

		if (width < 0.0f)
		{
			width = 0.0f;
		}

		if (height < 0.0f)
		{
			height = 0.0f;
		}

		if (range < 0.0f)
		{
			range = 0.0f;
		}

		ImGui::TextDisabled(
			"Direction: %.3f, %.3f, %.3f",
			transform.forward.x,
			transform.forward.y,
			transform.forward.z);

		ImGui::TextDisabled(
			"Right: %.3f, %.3f, %.3f",
			transform.right.x,
			transform.right.y,
			transform.right.z);

		ImGui::TreePop();
	}
}
