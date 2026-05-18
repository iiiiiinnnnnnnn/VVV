#pragma once

#include "Common.h"

struct DirectionalLight
{
	Vector3	direction = { 0, -1, 0 };
	Color	color = { 1, 1, 1 };
};

struct PointLight
{
	Vector3	position = { 0, 0, 0 };
	float	range = 10.0f;
	Color	color = { 1, 1, 1 };
};

struct SpotLight
{
	Vector3	position = { 0, 0, 0 };
	Vector3	direction = { 0, -1, 0 };
	Color	color = { 1, 1, 1 };
	float	range = 10.0f;
	float	innerConeAngle = DirectX::XMConvertToRadians(15.0f);
	float	outerConeAngle = DirectX::XMConvertToRadians(30.0f);
};

class LightManager
{
public:
	constexpr static int MaxPointLights = 4;
	constexpr static int MaxSpotLights = 4;

	// ディレクショナルライト設定
	void SetDirectionalLight(DirectionalLight& light) { directionalLight = light; }

	// ディレクショナルライト取得
	const DirectionalLight& GetDirectionalLight() const { return directionalLight; }

	// ポイントライト追加
	bool AddPointLight(PointLight& light)
	{
		if (pointLightCount >= MaxPointLights) return false;
		pointLights[pointLightCount++] = light;
		return true;
	}

	// ポイントライト取得
	const PointLight* GetPointLights() const { return pointLights; }

	// スポットライト追加
	bool AddSpotLight(SpotLight& light)
	{
		if (spotLightCount >= MaxSpotLights) return false;
		spotLights[spotLightCount++] = light;
		return true;
	}

	// スポットライト取得
	const SpotLight* GetSpotLights() const { return spotLights; }

	// アンビエントカラー設定
	void SetAmbientColor(const Color& color) { ambientColor = color; }

	// アンビエントカラー取得
	const Color& GetAmbientColor() const { return ambientColor; }

private:
	DirectionalLight	directionalLight;
	PointLight			pointLights[MaxPointLights];
	SpotLight			spotLights[MaxSpotLights];
	unsigned int 		pointLightCount = 0;
	unsigned int 		spotLightCount = 0;
	Color ambientColor = { 1, 1, 1 };
};
