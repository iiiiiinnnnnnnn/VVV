// Terrain.h

#pragma once

#include "Component.h"
#include "Common.h"

class Terrain : public Component
{
public:
	Terrain(Object* owner);
	~Terrain() override = default;

	void Update() override;
	void Render(const RenderContext& rc) override;
	void DrawGUI() override;

	float GetHeightByUV(float u, float v) const;

private:
	enum class BrushMode
	{
		Height,
		Blend,
	};

	struct TerrainVertex
	{
		Vector3 position;
		Vector3 normal;
		Vector2 texcoord;
	};

	struct CbTerrainObject
	{
		Matrix world;
		float terrainSize;
		float heightMapTexelSize;
		float dummy[2];
	};

	struct CbTerrainScene
	{
		Matrix viewProjection;
		Vector3 viewPosition;
		float dummy0;

		Vector3 directionalLightDirection;
		float dummy1;

		Color directionalLightColor;
		Color ambientColor;
	};

	struct CbTessellation
	{
		float edge_factor = 4.0f;
		float inner_factor = 4.0f;
		float height_scaler = 25.0f;
		float tilling_scale = 80.0f;
	};

	static constexpr int TerrainTextureWidth = 1024;
	static constexpr int TerrainTextureHeight = 1024;

private:
	void InitializeGpuResources();
	void CreateGridMesh(ID3D11Device* device);
	void CreateTerrainTexture(ID3D11Device* device);
	void UploadTerrainTexture(ID3D11DeviceContext* dc);
	void ClearTerrainTexture();

	void PaintByMouse(const RenderContext& rc);
	bool ScreenToTerrainUV(const RenderContext& rc, float& outU, float& outV) const;
	void ApplyBrush(float u, float v, float heightSign);

private:
	float terrainSize = 500.0f;
	int gridResolution = 64;
	UINT indexCount = 0;

	bool use_wire = false;
	CbTessellation tesselation_constant;

	Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;

	Microsoft::WRL::ComPtr<ID3D11Buffer> terrainObjectConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> terrainSceneConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> tesselationConstantBuffer;

	Microsoft::WRL::ComPtr<ID3D11VertexShader> terrainVertexShader;
	Microsoft::WRL::ComPtr<ID3D11HullShader> terrainHullShader;
	Microsoft::WRL::ComPtr<ID3D11DomainShader> terrainDomainShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> terrainPixelShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> terrainInputLayout;

	std::vector<Vector4> terrainPixels;
	bool terrainTextureDirty = true;
	bool is_terrain_texture_clear_color = true;
	Color terrain_texture_clear_color = {0.0f, 1.0f, 0.0f, 1.0f};

	Microsoft::WRL::ComPtr<ID3D11Texture2D> terrainTexture;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> terrainTextureShaderResourceView;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> terrainBaseColorShaderResourceView[3];

	bool use_brush = true;
	BrushMode brushMode = BrushMode::Height;
	int brush_size = 32;
	float heightBrushStrength = 0.02f;
	float blendBrushStrength = 0.08f;
	float blendTarget = 1.0f;
};