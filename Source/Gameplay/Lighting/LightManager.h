#pragma once
#include <deque>
#include <memory>
#include <vector>
#include <ranges>

#include "Core/Foundation/Common.h"
#include "Gameplay/Lighting/Light.h"
#include "Gameplay/Lighting/CbLightData.h"

// 普通に管理する用のライトデータ
class LightManager
{
public:
	void Update();
	void Render(const RenderContext& rc);
	void DrawDebug() const;
	void DrawGUI();

	DirectionalLight& GetDirectionalLight()
	{
		for (auto& light : directionalLights)
			if (!light.IsPendingDestroy()) return light;
		return directionalLights.front();
	}

	const DirectionalLight& GetDirectionalLight() const
	{
		for (auto& light : directionalLights)
			if (!light.IsPendingDestroy()) return light;
		return directionalLights.front();
	}

	auto GetPointLights()
	{
		return std::ranges::subrange(pointLights.begin(), pointLights.end());
	}

	const std::deque<PointLight>& GetPointLights() const
	{
		return pointLights;
	}

	auto GetSpotLights()
	{
		return std::ranges::subrange(spotLights.begin(), spotLights.end());
	}

	const std::deque<SpotLight>& GetSpotLights() const
	{
		return spotLights;
	}

	auto GetAreaLights()
	{
		return std::ranges::subrange(areaLights.begin(), areaLights.end());
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
		auto& directionalLight = GetDirectionalLight();
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

	}

	int GetLightCount() const;
	bool CanAddLight() const { return GetLightCount() < CbLightData::MaxLights; }
	auto GetDirectionalLights() { return std::ranges::subrange(directionalLights.begin(), directionalLights.end()); }
	const std::deque<DirectionalLight>& GetDirectionalLights() const { return directionalLights; }
	DirectionalLight* AddDirectionalLight();
	PointLight* AddPointLight();
	SpotLight* AddSpotLight();
	AreaLight* AddAreaLight();
	bool RegisterAttachedPointLight(const std::shared_ptr<PointLight>& light);
	void ClearAttachedPointLights() { attachedPointLights.clear(); }
	void ClearLights();

	void SetAmbientColor(const Color& color)
	{
		ambientColor = color;
	}

	CbLightData ConvertToCb() const;

private:
	std::deque<DirectionalLight> directionalLights{1};

	std::deque<PointLight> pointLights;
	std::vector<std::weak_ptr<PointLight>> attachedPointLights;
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
