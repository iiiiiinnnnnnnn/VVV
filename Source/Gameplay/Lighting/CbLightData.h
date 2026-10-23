#pragma once

#include "Core/Foundation/Common.h"

// コンスタントバッファに渡す用のライトデータ
struct CbLightData
{
	static constexpr int MaxLights = 150;
	static constexpr int MaxDirectionalLights = MaxLights;
	static constexpr int MaxPointLights = MaxLights;
	static constexpr int MaxSpotLights = MaxLights;
	static constexpr int MaxAreaLights = MaxLights;

	struct CbDirectionalLight
	{
		Vector3 direction;
		float DUMMY;

		Color color;
	} directionalLights[MaxDirectionalLights];

	struct CbPointLight
	{
		Vector3 position;
		float range;

		Color color;
	} pointLights[MaxPointLights];

	struct CbSpotLight
	{
		Vector3 position;
		float dummy0;

		Vector3 direction;
		float dummy1;

		Color color;

		float range;
		float innerConeAngle;
		float outerConeAngle;
		float dummy2;
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
	int directionalLightCount;
};

static_assert(sizeof(CbLightData) == 28832);
