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

// ライト管理
class LightData
{
public:
	constexpr static int MaxPointLights = 4;
	constexpr static int MaxSpotLights = 4;

	void SetDirectionalLight(DirectionalLight& light) { directionalLight = light; }
	const DirectionalLight& GetDirectionalLight() const { return directionalLight; }
	DirectionalLight& GetDirectionalLight() { return directionalLight; }

	bool AddPointLight(PointLight& light)
	{
		if (pointLightCount >= MaxPointLights) return false;
		pointLights[pointLightCount++] = light;
		return true;
	}
	const PointLight* GetPointLights() const { return pointLights; }
	PointLight* GetPointLights() { return pointLights; }

	bool AddSpotLight(SpotLight& light)
	{
		if (spotLightCount >= MaxSpotLights) return false;
		spotLights[spotLightCount++] = light;
		return true;
	}
	const SpotLight* GetSpotLights() const { return spotLights; }
	SpotLight* GetSpotLights() { return spotLights; }

	void SetAmbientColor(const Color& color) { ambientColor = color; }
	const Color& GetAmbientColor() const { return ambientColor; }
	Color& GetAmbientColor() { return ambientColor; }

	unsigned int GetPointLightCount() const { return pointLightCount; }
	unsigned int GetSpotLightCount()  const { return spotLightCount; }

	void DrawGUI()
	{
		ImGui::ColorPicker4("Ambient Color", &ambientColor.x);
		ImGui::Separator();
		ImGui::Text("Directional Light");
		ImGui::ColorPicker4("Directional Light Color", &directionalLight.color.x);
		ImGui::DragFloat3("Directional Light Direction", &directionalLight.direction.x, 0.1f);
		ImGui::Separator();
		for (unsigned int i = 0; i < pointLightCount; ++i)
		{
			ImGui::Text("Point Light %d", i);
			ImGui::ColorPicker4(("Point Light Color##" + std::to_string(i)).c_str(), &pointLights[i].color.x);
			ImGui::DragFloat3(("Point Light Position##" + std::to_string(i)).c_str(), &pointLights[i].position.x, 0.1f);
			ImGui::DragFloat(("Point Light Range##" + std::to_string(i)).c_str(), &pointLights[i].range, 0.1f, 0.0f, FLT_MAX);
			ImGui::Separator();
		}
		for (unsigned int i = 0; i < spotLightCount; ++i)
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
	}

private:
	DirectionalLight	directionalLight;
	PointLight			pointLights[MaxPointLights];
	SpotLight			spotLights[MaxSpotLights];
	unsigned int		pointLightCount = 0;
	unsigned int		spotLightCount = 0;
	Color				ambientColor = {1, 1, 1, 1};
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
	Color shadowColor = {0, 0, 0, 1};
	float shadowBias = 0.001f;
	int pcfKernelSize = 4;
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
