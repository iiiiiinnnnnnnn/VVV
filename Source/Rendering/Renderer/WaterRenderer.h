// WaterRenderer.h
#pragma once

#include <d3d11.h>
#include <memory>
#include <wrl.h>

#include "Core/Foundation/Common.h"
#include "Core/Object/Component.h"

class Texture;

// 水面専用の描画コンポーネント
class WaterRenderer : public Component
{
public:
	struct Settings
	{
		Color shallowColor = {0.08f, 0.42f, 0.58f, 0.62f};
		Color deepColor = {0.015f, 0.08f, 0.18f, 0.82f};
		float waveScale = 0.7f;
		float waveSpeed = 0.25f;
		float waveStrength = 0.12f;
		float fresnelPower = 4.0f;
		float fresnelStrength = 0.5f;
		float opacity = 0.68f;
		float shoreFadeDistance = 0.6f;
	};

	WaterRenderer(Object* owner);
	void OnRender(const RenderContext& rc) override;
	const char* GetDebugName() const override { return ICON_FA_WATER " Water Renderer"; }

	void SetSettings(const Settings& value) { settings = value; }
	const Settings& GetSettings() const { return settings; }

private:
	struct Vertex
	{
		Vector3 position;
		Vector2 uv;
	};

	struct ConstantBuffer
	{
		Matrix viewProjection;
		Matrix world;
		Matrix inverseViewProjection;
		Vector3 cameraPosition;
		float time;
		Color shallowColor;
		Color deepColor;
		float waveScale;
		float waveSpeed;
		float waveStrength;
		float fresnelPower;
		float fresnelStrength;
		float opacity;
		Vector2 screenSize;
		Vector2 windDirection;
		float textureTiling;
		float normalIntensity;
		Vector3 lightDirection;
		float shininess;
		Color lightColor;
		Color ambientColor;
		float shoreFadeDistance;
		float shorePadding[3];
	};

	Settings settings;
	Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
	Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
	std::shared_ptr<Texture> normalTexture;
	std::shared_ptr<Texture> heightTexture;
	std::shared_ptr<Texture> foamTexture;
	UINT vertexCount = 0;
};
