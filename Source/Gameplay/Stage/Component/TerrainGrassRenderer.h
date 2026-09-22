#pragma once

#include <d3d11.h>
#include <memory>
#include <wrl.h>

#include "Core/Foundation/Common.h"

struct RenderContext;
class Terrain;

// Terrain上の点をジオメトリシェーダーで草カードへ展開する描画クラス
class TerrainGrassRenderer
{
public:
	struct Settings
	{
		bool enabled = false;
		float density = 20.0f;
		float width = 0.75f;
		float height = 1.15f;
		float sizeVariation = 0.3f;
		float windStrength = 0.18f;
		float windSpeed = 1.35f;
		float drawDistance = 55.0f;
		Color tint = {0.58f, 0.78f, 0.48f, 1.0f};
	};

	TerrainGrassRenderer(ID3D11Device* device);
	void Rebuild(const Terrain& terrain, const Settings& settings);
	void Render(const RenderContext& rc, const Matrix& world, const Settings& settings,
		ID3D11ShaderResourceView* terrainDataView, float terrainSize);
	int GetTuftCount() const { return tuftCount; }

private:
	struct Vertex
	{
		Vector3 position;
		Vector3 normal;
		float random;
	};

	struct Constants
	{
		Matrix world;
		Matrix viewProjection;
		Vector3 cameraPosition;
		float time;
		float width;
		float height;
		float windStrength;
		float windSpeed;
		float sizeVariation;
		float drawDistance;
		float terrainSize;
		float padding;
		Color tint;
		Color fogColor;
		float fogStart;
		float fogEnd;
		float fogStrength;
		float fogEnabled;
	};

	Microsoft::WRL::ComPtr<ID3D11Device> device;
	Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
	Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
	Microsoft::WRL::ComPtr<ID3D11GeometryShader> geometryShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
	int tuftCount = 0;
};
