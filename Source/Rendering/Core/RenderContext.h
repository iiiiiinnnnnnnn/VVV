// RenderContext.h

#pragma once
#include <d3d11.h>
#include <wrl.h>

#include <array>

#include "Gameplay/Camera/Camera.h"
#include "Rendering/Core/RenderState.h"

class LightManager;

// レンダリング設定
struct RenderSettings
{
	bool showDebug = false;
	bool showComponentDebug = true;
	bool showColliderDebug = true;
	bool showNavMeshDebug = true;
	bool showLightDebug = true;
	bool wireframe = false;
};

// シャドウマップ用情報
struct ShadowMapData
{
	static constexpr int CascadeCount = 4;
	std::array<ID3D11ShaderResourceView*, CascadeCount> shadowMaps{};
	std::array<Matrix, CascadeCount> lightViewProjections{};
	Vector4 cascadeSplits = {};
	Color shadowColor = {0.2f, 0.2f, 0.2f, 1.0f};
	float shadowBias = 0.001f;
	int pcfKernelSize = 2;
};

// IBL用情報
struct IBLData
{
	ID3D11ShaderResourceView* diffuseIrradianceEnvironmentMap = nullptr;
	ID3D11ShaderResourceView* specularPremappingRadianceEnvironmentMap = nullptr;
	ID3D11ShaderResourceView* ggxLookUpTableMap = nullptr;
};

struct RenderContext
{
	ID3D11DeviceContext* deviceContext = nullptr;
	const RenderState* renderState = nullptr;
	const Camera* camera = nullptr;
	const LightManager* lightManager;

	RenderSettings			renderSettings;
	ShadowMapData			shadowMapData;
	IBLData					iblData;
};
