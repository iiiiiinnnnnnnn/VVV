// Terrain.h

#pragma once
#include <d3d11.h>
#include <wrl.h>

#include <filesystem>
#include <cstdint>
#include <string>
#include <vector>

#include "Component.h"
#include "CbLightData.h"

class Terrain : public Component
{
public:
	Terrain(Object* owner);
	~Terrain() override = default;

	void Update() override;
	void Render(const RenderContext& rc) override;
	void DrawGUI() override;
	const char* GetDebugName() const override { return ICON_FA_MOUNTAIN " Terrain"; }

	void RenderShadowMap(
		ID3D11DeviceContext* dc,
		const Matrix& lightViewProjection);

	float GetHeightByUV(float u, float v) const;
	float GetTerrainSize() const { return terrainSize; }
	int GetHeightMapWidth() const { return TerrainTextureWidth; }
	int GetHeightMapHeight() const { return TerrainTextureHeight; }
	int GetGridResolution() const { return gridResolution; }
	float GetTessellationEdgeFactor() const { return tesselation_constant.edge_factor; }
	float GetTessellationInnerFactor() const { return tesselation_constant.inner_factor; }
	float GetHeightScaler() const { return tesselation_constant.height_scaler; }
	uint64_t GetTerrainDataHash() const;
	std::filesystem::path GetColliderVertexPath() const;
	bool BuildGpuColliderMesh(
		float minX,
		float maxX,
		float minZ,
		float maxZ,
		std::vector<Vector3>& vertices,
		std::vector<uint32_t>& indices);

	bool SaveTerrainTexture(const std::string& filename);
	bool LoadTerrainTexture(const std::string& filename);

	bool AddBrushTexture(const std::string& filename);
	bool SetBrushTexture(int index);
	int GetBrushTextureIndex() const { return currentBrushIndex; }
	int GetBrushTextureCount() const { return static_cast<int>(brushes.size()); }

private:
	enum class BrushMode
	{
		RaiseLower,
		SetHeight,
		Paint,
	};

	static constexpr int MaxTerrainLayers = 16;

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
		float tilling_scale = 70.0f;
	};

	struct CbTerrainScene
	{
		Matrix viewProjection;
		Vector3 viewPosition;
		float dummy;
		CbLightData lightData;
	};

	struct CbTerrainColliderBuild
	{
		float terrainSize;
		float heightMapTexelSize;
		float heightScaler;
		float dummy0;
		int minGridX;
		int minGridZ;
		int segmentCountX;
		int segmentCountZ;
		int totalSegmentCountX;
		int totalSegmentCountZ;
		int vertexLineCount;
		int dummy1;
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

	struct TerrainLayer
	{
		std::string name;
		std::string baseColorPath;
		std::string normalPath;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> baseColorView;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> normalView;
	};

	struct CbTerrainLayer
	{
		int layerCount = 0;
		int dummy[3] = {};
	};

	static constexpr int TerrainTextureWidth = 1024;
	static constexpr int TerrainTextureHeight = 1024;

	void InitializeGpuResources();
	void CreateGridMesh(ID3D11Device* device);
	void BuildTerrainMesh(
		float minX,
		float maxX,
		float minZ,
		float maxZ,
		std::vector<TerrainVertex>& vertices,
		std::vector<uint32_t>& indices) const;
	void MarkTerrainMeshDirty();
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
	bool AddTerrainLayer(const std::string& baseColorPath, const std::string& normalPath);
	void DrawTerrainLayerGUI();
	float GetTerrainLayerValue(int layerIndex) const;
	void RebuildTerrainCollider();

private:
	float terrainSize = 500.0f;
	int gridResolution = 64;
	UINT indexCount = 0;

	CbTessellation tesselation_constant;

	Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
	bool terrainMeshDirty = true;

	Microsoft::WRL::ComPtr<ID3D11Buffer> shadowMapConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> materialConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> terrainObjectConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> terrainSceneConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> tesselationConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> terrainLayerConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> terrainColliderBuildConstantBuffer;

	Microsoft::WRL::ComPtr<ID3D11VertexShader> terrainVertexShader;
	Microsoft::WRL::ComPtr<ID3D11HullShader> terrainHullShader;
	Microsoft::WRL::ComPtr<ID3D11DomainShader> terrainDomainShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> terrainPixelShader;
	Microsoft::WRL::ComPtr<ID3D11ComputeShader> terrainColliderBuildComputeShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> terrainInputLayout;

	std::vector<Vector4> terrainPixels;
	bool terrainTextureDirty = true;
	bool is_terrain_texture_clear_color = true;
	Color terrain_texture_clear_color = {0.0f, 0.0f, 0.0f, 1.0f};

	Microsoft::WRL::ComPtr<ID3D11Texture2D> terrainTexture;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> terrainTextureShaderResourceView;

	std::vector<TerrainLayer> terrainLayers;
	int currentTerrainLayerIndex = 0;

	Color baseColor = {1.0f, 1.0f, 1.0f, 1.0f};
	Color emissiveColor = {0.1f, 0.1f, 0.1f, 1.0f};
	float metalness = 0.0f;
	float roughness = 0.2f;
	float occlusion = 1.0f;
	float occlusionStrength = 1.0f;
	float shadowStrength = 1.0f;

	bool use_brush = false;
	BrushMode brushMode = BrushMode::RaiseLower;
	int brush_size = 32;
	float heightBrushStrength = 0.02f;
	float setHeightValue = 0.0f;
	float paintOpacity = 0.08f;
	bool invertBrushMask = false;

	std::vector<TerrainBrush> brushes;
	int currentBrushIndex = -1;

	std::string terrainFilePath = "Data/Terrain/Maps/Default.dds";
	std::string terrainIoMessage;
	bool pendingColliderRebuild = false;
};
