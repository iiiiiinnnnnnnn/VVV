#pragma once

#include "Rendering/Core/MaterialMap.h"

struct PBRParameters
{
	inline static const MaterialParameter<Color> BaseColor{"PBR", "baseColor", Color(1, 1, 1, 1)};
	inline static const MaterialParameter<Color> EmissiveColor{"PBR", "emissiveColor", Color(0, 0, 0, 1)};
	inline static const MaterialParameter<float> Metalness{"PBR", "metalness", 0.0f};
	inline static const MaterialParameter<float> Roughness{"PBR", "roughness", 0.0f};
	inline static const MaterialParameter<float> Transmission{"PBR", "transmission", 0.0f};
	inline static const MaterialParameter<float> IndexOfRefraction{"PBR", "indexOfRefraction", 1.5f};
	inline static const MaterialParameter<float> RefractionDistance{"PBR", "refractionDistance", 0.18f};
	inline static const MaterialParameter<float> CrystalRoughness{"PBR", "crystalRoughness", 0.12f};
	inline static const MaterialParameter<float> Occlusion{"PBR", "occlusion", 1.0f};
	inline static const MaterialParameter<float> OcclusionStrength{"PBR", "occlusionStrength", 0.0f};
	inline static const MaterialParameter<float> ShadowStrength{"PBR", "shadowStrength", 1.0f};
	inline static const MaterialParameter<Color> FresnelColor{"PBR", "fresnelColor", Color(1, 1, 1, 0)};
	inline static const MaterialParameter<float> FresnelPower{"PBR", "fresnelPower", 0.0f};
	inline static const MaterialParameter<float> FresnelStrength{"PBR", "fresnelStrength", 0.0f};
	inline static const MaterialParameter<int> IsFlatShading{"PBR", "isFlatShading", 0};

	static MaterialMap Defaults()
	{
		MaterialMap parameters;
		parameters.Get(BaseColor);
		parameters.Get(EmissiveColor);
		parameters.Get(Metalness);
		parameters.Get(Roughness);
		parameters.Get(Transmission);
		parameters.Get(IndexOfRefraction);
		parameters.Get(RefractionDistance);
		parameters.Get(CrystalRoughness);
		parameters.Get(Occlusion);
		parameters.Get(OcclusionStrength);
		parameters.Get(ShadowStrength);
		parameters.Get(FresnelColor);
		parameters.Get(FresnelPower);
		parameters.Get(FresnelStrength);
		parameters.Get(IsFlatShading);
		return parameters;
	}
};

struct FunkyParameters
{
	inline static const MaterialParameter<float> LineWidth{"Funky", "lineWidth", 4.0f};
	inline static const MaterialParameter<float> Speed{"Funky", "speed", 30.0f};

	static MaterialMap Defaults()
	{
		MaterialMap parameters;
		parameters.Get(LineWidth);
		parameters.Get(Speed);
		return parameters;
	}
};
