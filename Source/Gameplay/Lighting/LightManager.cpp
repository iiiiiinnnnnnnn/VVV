// LightManager.cpp

#include "Gameplay/Lighting/LightManager.h"

#include <algorithm>
#include "Gameplay/Lighting/CbLightData.h"
#include "Rendering/Core/Graphics.h"
#include "Rendering/Renderer/ShapeRenderer.h"
#include "Rendering/Renderer/PrimitiveRenderer.h"
#include "imgui.h"

void LightManager::Update()
{
	directionalLight.Update();

	for (PointLight& pointLight : pointLights)
	{
		if (!pointLight.IsPendingDestroy())
			pointLight.Update();
	}

	for (SpotLight& spotLight : spotLights)
	{
		if (!spotLight.IsPendingDestroy())
			spotLight.Update();
	}

	for (AreaLight& areaLight : areaLights)
	{
		if (!areaLight.IsPendingDestroy())
			areaLight.Update();
	}
}

void LightManager::Render(const RenderContext& rc)
{
	directionalLight.Render(rc);

	for (PointLight& pointLight : pointLights)
	{
		if (!pointLight.IsPendingDestroy())
			pointLight.Render(rc);
	}
	for (SpotLight& spotLight : spotLights)
	{
		if (!spotLight.IsPendingDestroy())
			spotLight.Render(rc);
	}
	for (AreaLight& areaLight : areaLights)
	{
		if (!areaLight.IsPendingDestroy())
			areaLight.Render(rc);
	}
}

void LightManager::DrawDebug() const
{
	ShapeRenderer* renderer =
		Game::Graphics::Instance().GetShapeRenderer();
	PrimitiveRenderer* primitiveRenderer =
		Game::Graphics::Instance().GetPrimitiveRenderer();
	if (!renderer || !primitiveRenderer) return;

	for (const PointLight& pointLight : pointLights)
	{
		if (!pointLight.IsActive() ||
			pointLight.IsPendingDestroy()) continue;

		Color color = pointLight.GetColor();
		color.w = 1.0f;
		renderer->DrawSphere(
			pointLight.transform.position,
			0.15f,
			color);
		renderer->DrawSphere(
			pointLight.transform.position,
			pointLight.GetRange(),
			color);
	}

	for (const AreaLight& areaLight : areaLights)
	{
		if (!areaLight.IsActive() ||
			areaLight.IsPendingDestroy()) continue;

		const Color color(
			1.0f,
			0.55f,
			0.05f,
			1.0f);
		const Vector3 angle =
			areaLight.transform.rotation.ToEuler();
		const float halfWidth =
			std::max(areaLight.GetWidth(), 0.0f) *
			0.5f;
		const float halfHeight =
			std::max(areaLight.GetHeight(), 0.0f) *
			0.5f;
		const float range =
			std::max(areaLight.GetRange(), 0.0f);

		renderer->DrawBox(
			areaLight.transform.position,
			angle,
			Vector3(
				halfWidth,
				halfHeight,
				0.025f),
			color);

		Color rangeColor = color;
		rangeColor.w = 0.45f;
		const Vector3 rangeEnd =
			areaLight.transform.position -
			areaLight.transform.forward *
			range;
		primitiveRenderer->DrawLine(
			areaLight.transform.position,
			rangeEnd,
			color,
			rangeColor);
		renderer->DrawSphere(
			rangeEnd,
			0.1f,
			color);
	}
}

void LightManager::DrawGUI()
{
	const auto visibleLightCount =
		[](const auto& lights)
		{
			return static_cast<int>(
				std::count_if(
					lights.begin(),
					lights.end(),
					[](const auto& light)
					{
						return !light.IsPendingDestroy();
					}));
		};

	if (ImGui::CollapsingHeader(
		"Environment",
		ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::ColorEdit4(
			"Ambient Color",
			&ambientColor.x,
			ImGuiColorEditFlags_Float);
	}

	if (ImGui::CollapsingHeader(
		"Directional Light",
		ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::PushID(&directionalLight);
		directionalLight.DrawGUI();
		ImGui::PopID();
	}

	if (ImGui::CollapsingHeader(
		"Point Lights",
		ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text(
			"Count: %d / %d",
			visibleLightCount(pointLights),
			CbLightData::MaxPointLights);

		for (int i = 0;
			i < static_cast<int>(pointLights.size());
			++i)
		{
			if (pointLights[i].IsPendingDestroy()) continue;

			ImGui::PushID(&pointLights[i]);
			pointLights[i].DrawGUI();
			ImGui::PopID();
		}
	}

	if (ImGui::CollapsingHeader(
		"Spot Lights",
		ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text(
			"Count: %d / %d",
			visibleLightCount(spotLights),
			CbLightData::MaxSpotLights);

		for (int i = 0;
			i < static_cast<int>(spotLights.size());
			++i)
		{
			if (spotLights[i].IsPendingDestroy()) continue;

			ImGui::PushID(&spotLights[i]);
			spotLights[i].DrawGUI();
			ImGui::PopID();
		}
	}

	if (ImGui::CollapsingHeader(
		"Area Lights",
		ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text(
			"Count: %d / %d",
			visibleLightCount(areaLights),
			CbLightData::MaxAreaLights);

		for (int i = 0;
			i < static_cast<int>(areaLights.size());
			++i)
		{
			if (areaLights[i].IsPendingDestroy()) continue;

			ImGui::PushID(&areaLights[i]);
			areaLights[i].DrawGUI();
			ImGui::PopID();
		}
	}
}

CbLightData LightManager::ConvertToCb() const
{
	CbLightData cbLightData{};

	cbLightData.ambientColor =
		ambientColor;

	// Directional Light
	{
		Vector3 direction =
			Vector3::TransformNormal(
			Vector3::UnitZ,
			Matrix::CreateFromQuaternion(
			directionalLight.transform.rotation));

		if (direction.LengthSquared() > 0.000001f)
		{
			direction.Normalize();
		}
		else
		{
			direction = Vector3::UnitZ;
		}

		cbLightData.directionalLight.direction =
			direction;

		if (directionalLight.IsActive())
		{
			cbLightData.directionalLight.color =
				directionalLight.GetColor();

			cbLightData.directionalLight.color.w =
				directionalLight.GetIntensity();
		}
		else
		{
			cbLightData.directionalLight.color =
			{ 0.0f, 0.0f, 0.0f, 0.0f };
		}
	}

	// Point Lights
	for (const PointLight& pointLight : pointLights)
	{
		if (!pointLight.IsActive() ||
			pointLight.IsPendingDestroy())
		{
			continue;
		}

		if (cbLightData.pointLightCount >=
			CbLightData::MaxPointLights)
		{
			break;
		}

		CbLightData::CbPointLight& cbPointLight =
			cbLightData.pointLights[
				cbLightData.pointLightCount];

		cbPointLight.position =
			pointLight.transform.position;

		cbPointLight.color =
			pointLight.GetColor();

		cbPointLight.color.w =
			pointLight.GetIntensity();

		cbPointLight.range =
			pointLight.GetRange();

		++cbLightData.pointLightCount;
	}

	// Spot Lights
	for (const SpotLight& spotLight : spotLights)
	{
		if (!spotLight.IsActive() ||
			spotLight.IsPendingDestroy())
		{
			continue;
		}

		if (cbLightData.spotLightCount >=
			CbLightData::MaxSpotLights)
		{
			break;
		}

		CbLightData::CbSpotLight& cbSpotLight =
			cbLightData.spotLights[
				cbLightData.spotLightCount];

		Vector3 direction =
			Vector3::TransformNormal(
			Vector3::UnitZ,
			Matrix::CreateFromQuaternion(
			spotLight.transform.rotation));

		if (direction.LengthSquared() > 0.000001f)
		{
			direction.Normalize();
		}
		else
		{
			direction = Vector3::UnitZ;
		}

		cbSpotLight.position =
			spotLight.transform.position;

		cbSpotLight.direction =
			direction;

		cbSpotLight.color =
			spotLight.GetColor();

		cbSpotLight.color.w =
			spotLight.GetIntensity();

		cbSpotLight.range =
			spotLight.GetRange();

		cbSpotLight.innerConeAngle =
			spotLight.GetInnerConeAngle();

		cbSpotLight.outerConeAngle =
			spotLight.GetOuterConeAngle();

		++cbLightData.spotLightCount;
	}

	// Area Lights
	for (const AreaLight& areaLight : areaLights)
	{
		if (!areaLight.IsActive() ||
			areaLight.IsPendingDestroy())
		{
			continue;
		}

		if (cbLightData.areaLightCount >=
			CbLightData::MaxAreaLights)
		{
			break;
		}

		CbLightData::CbAreaLight& cbAreaLight =
			cbLightData.areaLights[
				cbLightData.areaLightCount];

		Matrix rotationMatrix =
			Matrix::CreateFromQuaternion(
			areaLight.transform.rotation);

		Vector3 direction =
			Vector3::TransformNormal(
			Vector3::UnitZ,
			rotationMatrix);

		Vector3 right =
			Vector3::TransformNormal(
			Vector3::UnitX,
			rotationMatrix);

		if (direction.LengthSquared() > 0.000001f)
		{
			direction.Normalize();
		}
		else
		{
			direction = Vector3::UnitZ;
		}

		if (right.LengthSquared() > 0.000001f)
		{
			right.Normalize();
		}
		else
		{
			right = Vector3::UnitX;
		}

		cbAreaLight.position =
			areaLight.transform.position;

		cbAreaLight.direction =
			direction;

		cbAreaLight.right =
			right;

		cbAreaLight.width =
			areaLight.GetWidth();

		cbAreaLight.height =
			areaLight.GetHeight();

		cbAreaLight.range =
			areaLight.GetRange();

		cbAreaLight.color =
			areaLight.GetColor();

		cbAreaLight.color.w =
			areaLight.GetIntensity();

		++cbLightData.areaLightCount;
	}

	return cbLightData;
}
