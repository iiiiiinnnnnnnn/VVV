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

	void RenderShadowMap(
		ID3D11DeviceContext* dc,
		const Matrix& lightViewProjection);

	float GetHeightByUV(float u, float v) const;
	float GetTerrainSize() const { return terrainSize; }

	bool SaveTerrainTexture(const std::string& filename);
	bool LoadTerrainTexture(const std::string& filename);

	bool AddBrushTexture(const std::string& filename);
	bool SetBrushTexture(int index);
	int GetBrushTextureIndex() const { return currentBrushIndex; }
	int GetBrushTextureCount() const { return static_cast<int>(brushes.size()); }

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

	struct CbShadowMap
	{
		Matrix lightViewProjection;
		Color shadowColor;
		float shadowBias;
		int pcfKernelSize;
		float dummy[2];
	};

	struct CbMaterial
	{
		Color baseColor;
		Color emissiveColor;

		float metalness;
		float roughness;
		float occlusion;
		float occlusionStrength;

		float shadowStrength;
		int useMetalnessTexture;
		int useRoughnessTexture;
		int useOcclusionTexture;
	};

	struct CbTerrainObject
	{
		Matrix world;
		float terrainSize;
		float heightMapTexelSize;
		float dummy[2];
	};

	struct CbTessellation
	{
		float edge_factor = 4.0f;
		float inner_factor = 4.0f;
		float height_scaler = 25.0f;
		float tilling_scale = 12.0f;
	};

	struct CbDirectionalLight
	{
		Vector3 direction;
		float dummy;
		Color color;
	};

	struct CbPointLight
	{
		Vector3 position;
		float range;
		Color color;
	};

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
	};

	struct CbAreaLight
	{
		Vector3 position;
		float width;
		Vector3 direction;
		float height;
		Vector3 right;
		float range;
		Color color;
	};

	struct CbLightManager
	{
		CbDirectionalLight directionalLight;
		CbPointLight pointLights[LightData::MaxPointLights];
		CbSpotLight spotLights[LightData::MaxSpotLights];
		CbAreaLight areaLights[LightData::MaxAreaLights];
		Color ambientColor;
		int pointLightCount;
		int spotLightCount;
		int areaLightCount;
		float dummy;
	};

	struct CbTerrainScene
	{
		Matrix viewProjection;
		Vector3 viewPosition;
		float dummy;
		CbLightManager lightManager;
	};

	struct TerrainBrush
	{
		std::string name;
		std::string filepath;
		int width = 0;
		int height = 0;
		std::vector<float> mask;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderResourceView;
	};

	static constexpr int TerrainTextureWidth = 1024;
	static constexpr int TerrainTextureHeight = 1024;

	void InitializeGpuResources();
	void CreateGridMesh(ID3D11Device* device);
	void CreateTerrainTexture(ID3D11Device* device);
	void UploadTerrainTexture(ID3D11DeviceContext* dc);
	void ClearTerrainTexture();

	void UpdateTerrainObjectConstantBuffer(ID3D11DeviceContext* dc);
	void UpdateTerrainSceneConstantBuffer(
		ID3D11DeviceContext* dc,
		const RenderContext& rc);
	void UpdateShadowConstantBuffer(
		ID3D11DeviceContext* dc,
		const Matrix& lightViewProjection,
		const Color& shadowColor,
		float shadowBias,
		int pcfKernelSize);
	void UpdateMaterialConstantBuffer(ID3D11DeviceContext* dc);

	void PaintByMouse(const RenderContext& rc);
	bool ScreenToTerrainUV(const RenderContext& rc, float& outU, float& outV) const;
	void ApplyBrush(float u, float v, float heightSign);

	const TerrainBrush* GetCurrentBrush() const;
	float SampleBrushMask(float u, float v) const;
	void DrawBrushGUI();
	void RebuildTerrainCollider();

private:
	float terrainSize = 500.0f;
	int gridResolution = 64;
	UINT indexCount = 0;

	bool use_wire = false;
	CbTessellation tesselation_constant;

	Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;

	Microsoft::WRL::ComPtr<ID3D11Buffer> shadowMapConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> materialConstantBuffer;
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

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> terrainLayer_Stone;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> terrainLayer_Stone_n;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> terrainLayer_Dirt;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> terrainLayer_Dirt_n;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> terrainLayer_Grass;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> terrainLayer_Grass_n;

	Color baseColor = {1.0f, 1.0f, 1.0f, 1.0f};
	Color emissiveColor = {0.0f, 0.0f, 0.0f, 1.0f};
	float metalness = 0.0f;
	float roughness = 0.85f;
	float occlusion = 1.0f;
	float occlusionStrength = 1.0f;
	float shadowStrength = 1.0f;

	bool use_brush = false;
	BrushMode brushMode = BrushMode::Height;
	int brush_size = 32;
	float heightBrushStrength = 0.02f;
	float blendBrushStrength = 0.08f;
	float blendTarget = 1.0f;
	bool invertBrushMask = false;

	std::vector<TerrainBrush> brushes;
	int currentBrushIndex = -1;
	std::string brushAddPath = "Data/Terrain/brush_default.png";

	std::string terrainFilePath = "Data/Terrain/Maps/Default.dds";
	std::string terrainIoMessage;
	bool pendingColliderRebuild = false;
};
