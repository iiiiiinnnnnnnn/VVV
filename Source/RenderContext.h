// RenderContext.h

#pragma once

#include "Camera.h"
#include "RenderState.h"
#include <imgui.h>

struct DirectionalLight
{
	Vector3	direction = {0, -1, 0};
	Color	color = {1, 1, 1, 1};
};

struct PointLight
{
	Vector3	position = {0, 0, 0};
	float	range = 10.0f;
	Color	color = {1, 1, 1, 1};
};

struct SpotLight
{
	Vector3	position = {0, 0, 0};
	Vector3	direction = {0, -1, 0};
	Color	color = {1, 1, 1, 1};
	float	range = 10.0f;
	float	innerConeAngle = DirectX::XMConvertToRadians(15.0f);
	float	outerConeAngle = DirectX::XMConvertToRadians(30.0f);
};

struct AreaLight
{
	Vector3 position = {0, 0, 0};
	Vector3 direction = {0, -1, 0};  // 法線方向
	Color   color = {1, 1, 1, 1};
	Vector3 right = {1, 0, 0};   // 矩形のX軸
	float   width = 1.0f;
	float   height = 1.0f;
	float   range = 10.0f;
};

// ライト管理
class LightData
{
public:
	constexpr static int MaxPointLights = 32;
	constexpr static int MaxSpotLights = 32;
	constexpr static int MaxAreaLights = 32;

	// ディレクショナルライト
	void SetDirectionalLight(DirectionalLight& light) { directionalLight = light; }
	const DirectionalLight& GetDirectionalLight() const { return directionalLight; }
	DirectionalLight& GetDirectionalLight() { return directionalLight; }

	// ポイントライト
	bool AddPointLight(PointLight& light)
	{
		if (pointLights.size() >= MaxPointLights) return false;
		pointLights.push_back(light);
		return true;
	}
	const std::vector<PointLight>& GetPointLights() const { return pointLights; }
	std::vector<PointLight>& GetPointLights() { return pointLights; }

	// スポットライト
	bool AddSpotLight(SpotLight& light)
	{
		if (spotLights.size() >= MaxSpotLights) return false;
		spotLights.push_back(light);
		return true;
	}
	const std::vector<SpotLight>& GetSpotLights() const { return spotLights; }
	std::vector<SpotLight>& GetSpotLights() { return spotLights; }

	// エリアライト
	bool AddAreaLight(AreaLight& light)
	{
		if (areaLights.size() >= MaxAreaLights) return false;
		areaLights.push_back(light);
		return true;
	}
	const std::vector<AreaLight>& GetAreaLights() const { return areaLights; }
	std::vector<AreaLight>& GetAreaLights() { return areaLights; }

	// アンビエント
	void SetAmbientColor(const Color& color) { ambientColor = color; }
	const Color& GetAmbientColor() const { return ambientColor; }
	Color& GetAmbientColor() { return ambientColor; }

	void DrawGUI()
	{
		ImGui::ColorPicker4("Ambient Color", &ambientColor.x);
		ImGui::Separator();
		ImGui::Text("Directional Light");
		ImGui::ColorPicker4("Directional Light Color", &directionalLight.color.x);
		ImGui::DragFloat3("Directional Light Direction", &directionalLight.direction.x, 0.1f);
		ImGui::Separator();
		for (unsigned int i = 0; i < pointLights.size(); ++i)
		{
			ImGui::Text("Point Light %d", i);
			ImGui::ColorPicker4(("Point Light Color##" + std::to_string(i)).c_str(), &pointLights[i].color.x);
			ImGui::DragFloat3(("Point Light Position##" + std::to_string(i)).c_str(), &pointLights[i].position.x, 0.1f);
			ImGui::DragFloat(("Point Light Range##" + std::to_string(i)).c_str(), &pointLights[i].range, 0.1f, 0.0f, FLT_MAX);
			ImGui::Separator();
		}
		for (unsigned int i = 0; i < spotLights.size(); ++i)
		{
			ImGui::Text("Spot Light %d", i);
			ImGui::ColorPicker4(("Spot Light Color##" + std::to_string(i)).c_str(), &spotLights[i].color.x);
			ImGui::DragFloat3(("Spot Light Position##" + std::to_string(i)).c_str(), &spotLights[i].position.x, 0.1f);
			ImGui::DragFloat3(("Spot Light Direction##" + std::to_string(i)).c_str(), &spotLights[i].direction.x, 0.1f);
			ImGui::DragFloat(("Spot Light Range##" + std::to_string(i)).c_str(), &spotLights[i].range, 0.1f, 0.0f, FLT_MAX);
			ImGui::DragFloat(("Spot Light Inner Cone Angle##" + std::to_string(i)).c_str(), &spotLights[i].innerConeAngle, 0.1f, 0.0f, DirectX::XM_PI / 2);
			ImGui::DragFloat(("Spot Light Outer Cone Angle##" + std::to_string(i)).c_str(), &spotLights[i].outerConeAngle, 0.1f, 0.0f, DirectX::XM_PI / 2);
			ImGui::Separator();
		}
		for (unsigned int i = 0; i < areaLights.size(); ++i)
		{
			ImGui::Text("Area Light %d", i);
			ImGui::ColorPicker4(("Area Light Color##" + std::to_string(i)).c_str(), &areaLights[i].color.x);
			ImGui::DragFloat3(("Area Light Position##" + std::to_string(i)).c_str(), &areaLights[i].position.x, 0.1f);
			ImGui::DragFloat3(("Area Light Direction##" + std::to_string(i)).c_str(), &areaLights[i].direction.x, 0.1f);
			ImGui::DragFloat3(("Area Light Right##" + std::to_string(i)).c_str(), &areaLights[i].right.x, 0.1f);
			ImGui::DragFloat(("Area Light Width##" + std::to_string(i)).c_str(), &areaLights[i].width, 0.1f, 0.0f, FLT_MAX);
			ImGui::DragFloat(("Area Light Height##" + std::to_string(i)).c_str(), &areaLights[i].height, 0.1f, 0.0f, FLT_MAX);
			ImGui::DragFloat(("Area Light Range##" + std::to_string(i)).c_str(), &areaLights[i].range, 0.1f, 0.0f, FLT_MAX);
			ImGui::Separator();
		}
	}

private:
	DirectionalLight		directionalLight;
	std::vector<PointLight>	pointLights;
	std::vector<SpotLight>	spotLights;
	std::vector<AreaLight>	areaLights;
	Color					ambientColor = {1, 1, 1, 1};
};

// レンダリング設定
struct RenderSettings
{
	bool showDebug = false;
	bool wireframe = false;
};

// シャドウマップ用情報
struct ShadowMapData
{
	ID3D11ShaderResourceView* shadowMap = nullptr;
	Matrix lightViewProjection;
	Color shadowColor = {0, 0, 0, 0.5f};
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

	LightData				lightData;
	RenderSettings			renderSettings;
	ShadowMapData			shadowMapData;
	IBLData					iblData;
};
