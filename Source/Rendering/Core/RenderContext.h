#pragma once
#include <d3d11.h>
#include <wrl.h>

#include <array>

#include "Gameplay/Camera/Camera.h"

class LightManager;
class DepthTarget;
namespace Game { class Graphics; }

// レンダリング設定
enum class ShadingMode { Shading, Unlit, Solid, Wire, SolidAndWire };

struct RenderSettings
{
	bool showDebug = false;
	bool showComponentDebug = true;
	bool showColliderDebug = true;
	bool showNavMeshDebug = true;
	bool showLightDebug = true;
	bool showGrass = true;
	bool showFog = true;
	ShadingMode shadingMode = ShadingMode::Shading;
	bool pointMaterialTextures = false;
	bool distanceFogEnabled = true;
	float distanceFogStart = 18.0f;
	float distanceFogEnd = 65.0f;
	float distanceFogStrength = 0.9f;
	Color distanceFogColor = {0.38f, 0.46f, 0.54f, 1.0f};
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

// 描画に必要な参照と設定を渡す。GPUリソースの生成・所有・Bindは行わない。
struct RenderContext
{
	ID3D11DeviceContext* deviceContext = nullptr;
	DepthTarget* depthTarget = nullptr;
	const Game::Graphics* graphics = nullptr;
	const Camera* camera = nullptr;
	const LightManager* lightManager = nullptr;

	RenderSettings			renderSettings;
	ShadowMapData			shadowMapData;
	IBLData					iblData;
	float underwaterBlend = 0.0f;
	Matrix underwaterInverseWorld = Matrix::Identity;
};
