// LightManager.h

#pragma once
#include <deque>

#include "Core/Foundation/Common.h"
#include "Gameplay/Lighting/Light.h"
#include "Gameplay/Lighting/CbLightData.h"

// 普通に管理する用のライトデータ
class LightManager
{
public:
	void Update();
	void Render(const RenderContext& rc);
	void DrawGUI();

	DirectionalLight& GetDirectionalLight()
	{
		return directionalLight;
	}

	const DirectionalLight& GetDirectionalLight() const
	{
		return directionalLight;
	}

	std::deque<PointLight>& GetPointLights()
	{
		return pointLights;
	}

	const std::deque<PointLight>& GetPointLights() const
	{
		return pointLights;
	}

	std::deque<SpotLight>& GetSpotLights()
	{
		return spotLights;
	}

	const std::deque<SpotLight>& GetSpotLights() const
	{
		return spotLights;
	}

	std::deque<AreaLight>& GetAreaLights()
	{
		return areaLights;
	}

	const std::deque<AreaLight>& GetAreaLights() const
	{
		return areaLights;
	}

	Color& GetAmbientColor()
	{
		return ambientColor;
	}

	const Color& GetAmbientColor() const
	{
		return ambientColor;
	}

	void SetDirectionalLight(
		const DirectionalLight& light)
	{
		directionalLight.SetName(
			light.GetName());

		directionalLight.SetActive(
			light.IsActive());

		directionalLight.SetColor(
			light.GetColor());

		directionalLight.SetIntensity(
			light.GetIntensity());

		directionalLight.transform =
			light.transform;

		directionalLight.transform.Update();
	}

	PointLight& AddPointLight()
	{
		pointLights.emplace_back();
		return pointLights.back();
	}

	SpotLight& AddSpotLight()
	{
		spotLights.emplace_back();
		return spotLights.back();
	}

	AreaLight& AddAreaLight()
	{
		areaLights.emplace_back();
		return areaLights.back();
	}

	void SetAmbientColor(const Color& color)
	{
		ambientColor = color;
	}

	CbLightData ConvertToCb() const;

private:
	DirectionalLight directionalLight;

	std::deque<PointLight> pointLights;
	std::deque<SpotLight> spotLights;
	std::deque<AreaLight> areaLights;

	Color ambientColor =
	{
		1.0f,
		1.0f,
		1.0f,
		1.0f
	};
};

