// LightManager.cpp

#include "LightManager.h"
#include "CbLightData.h"

void LightManager::Update()
{
	directionalLight.Update();

	for (PointLight& pointLight : pointLights)
	{
		pointLight.Update();
	}

	for (SpotLight& spotLight : spotLights)
	{
		spotLight.Update();
	}

	for (AreaLight& areaLight : areaLights)
	{
		areaLight.Update();
	}
}

void LightManager::Render(const RenderContext& rc)
{
	directionalLight.Render(rc);

	for (PointLight& pointLight : pointLights)
	{
		pointLight.Render(rc);
	}
	for (SpotLight& spotLight : spotLights)
	{
		spotLight.Render(rc);
	}
	for (AreaLight& areaLight : areaLights)
	{
		areaLight.Render(rc);
	}
}

void LightManager::DrawGUI()
{
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
			static_cast<int>(pointLights.size()),
			CbLightData::MaxPointLights);

		for (int i = 0;
			i < static_cast<int>(pointLights.size());
			++i)
		{
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
			static_cast<int>(spotLights.size()),
			CbLightData::MaxSpotLights);

		for (int i = 0;
			i < static_cast<int>(spotLights.size());
			++i)
		{
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
			static_cast<int>(areaLights.size()),
			CbLightData::MaxAreaLights);

		for (int i = 0;
			i < static_cast<int>(areaLights.size());
			++i)
		{
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
		if (!pointLight.IsActive())
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

		cbPointLight.range =
			pointLight.GetRange();

		++cbLightData.pointLightCount;
	}

	// Spot Lights
	for (const SpotLight& spotLight : spotLights)
	{
		if (!spotLight.IsActive())
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
		if (!areaLight.IsActive())
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

		++cbLightData.areaLightCount;
	}

	return cbLightData;
}
