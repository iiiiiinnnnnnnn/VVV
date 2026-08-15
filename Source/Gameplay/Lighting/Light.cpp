#include "Gameplay/Lighting/Light.h"
#include "imgui.h"

void Light::DrawGUI()
{
	// オブジェクトとトランスフォーム
	Object::DrawGUI();

	transform.DrawGUI();

	// 全ライト共通の色と強度
	if (ImGui::TreeNode((const char*)u8"ライト設定"))
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

		ImGui::TreePop();
	}
}

void DirectionalLight::DrawGUI()
{
	Light::DrawGUI();

	// ディレクショナルライトの方向
	if (ImGui::TreeNode((const char*)u8"方向"))
	{
		ImGui::TextDisabled(
			(const char*)u8"向き: %.3f, %.3f, %.3f",
			transform.forward.x,
			transform.forward.y,
			transform.forward.z);

		ImGui::TreePop();
	}
}

void PointLight::DrawGUI()
{
	Light::DrawGUI();

	// ポイントライトの範囲
	if (ImGui::TreeNode((const char*)u8"ポイントライト設定"))
	{
		ImGui::DragFloat(
			(const char*)u8"範囲",
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

	// スポットライトの範囲とコーン
	if (ImGui::TreeNode((const char*)u8"スポットライト設定"))
	{
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
			(const char*)u8"向き: %.3f, %.3f, %.3f",
			transform.forward.x,
			transform.forward.y,
			transform.forward.z);

		ImGui::TreePop();
	}
}

void AreaLight::DrawGUI()
{
	Light::DrawGUI();

	// エリアライトの大きさと範囲
	if (ImGui::TreeNode((const char*)u8"エリアライト設定"))
	{
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
			(const char*)u8"向き: %.3f, %.3f, %.3f",
			transform.forward.x,
			transform.forward.y,
			transform.forward.z);

		ImGui::TextDisabled(
			(const char*)u8"右方向: %.3f, %.3f, %.3f",
			transform.right.x,
			transform.right.y,
			transform.right.z);

		ImGui::TreePop();
	}
}
