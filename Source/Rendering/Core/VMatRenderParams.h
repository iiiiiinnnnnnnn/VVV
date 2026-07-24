// VMatRenderParams.h
#pragma once

#include <array>
#include <optional>
#include <string>
#include <unordered_map>

#include "Core/Foundation/Common.h"

struct VMatMaterialParams
{
	std::optional<Color> baseColor;
	std::optional<Color> emissionColor;
	std::optional<Color> fresnelColor;
	std::optional<float> metalness;
	std::optional<float> roughness;
	std::optional<float> occlusion;
	std::optional<float> occlusionStrength;
	std::optional<float> shadowStrength;
	std::optional<float> fresnelPower;
	std::optional<float> fresnelStrength;
	std::optional<bool> isFlatShading;
	std::optional<bool> useBaseColorTexture;
};

struct VMatDamageHoleParams
{
	static constexpr int MaxCount = 8;

	std::array<Vector4, MaxCount> holes{};
	std::array<Vector4, MaxCount> directions{};
	int count = 0;
	float edgeWidth = 1.5f;
	float depth = 0.4f;
};

struct VMatRenderParams
{
	std::unordered_map<std::string, VMatMaterialParams> materials;
	VMatDamageHoleParams damageHoles;
};
