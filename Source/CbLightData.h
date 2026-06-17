// CbLightData.h

#pragma once

#include "Common.h"

// コンスタントバッファに渡す用のライトデータ
struct CbLightData
{
	static constexpr int MaxPointLights = 32;
	static constexpr int MaxSpotLights = 32;
	static constexpr int MaxAreaLights = 32;

	struct CbDirectionalLight
	{
		Vector3 direction;
		float DUMMY;

		Color color;
	} directionalLight;

	struct CbPointLight
	{
		Vector3 position;
		float range;

		Color color;
	} pointLights[MaxPointLights];

	struct CbSpotLight
	{
		Vector3 position;
		float DUMMY;

		Vector3 direction;
		float DUMMY;

		Color color;

		float range;
		float innerConeAngle;
		float outerConeAngle;
		float DUMMY;
	} spotLights[MaxSpotLights];

	struct CbAreaLight
	{
		Vector3 position;
		float width;

		Vector3 direction;
		float height;

		Vector3 right;
		float range;

		Color color;
	} areaLights[MaxAreaLights];

	Color ambientColor;

	int pointLightCount;
	int spotLightCount;
	int areaLightCount;
	float DUMMY;
};