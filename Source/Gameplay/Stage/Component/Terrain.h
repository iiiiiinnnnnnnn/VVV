#pragma once

#include <d3d11.h>
#include <wrl.h>
#include <DirectXTex.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "Core/Object/Component.h"
#include "Gameplay/Lighting/CbLightData.h"
#include "Gameplay/Stage/Component/TerrainGrassRenderer.h"
#include "Rendering/Core/RenderContext.h"

class Terrain : public Component
{
public:
	// 基本処理

	Terrain(Object* owner);
	~Terrain() override = default;

	void Update() override;
	void Render(const RenderContext& rc) override;
	void DrawGUI() override;
	const char* GetDebugName() const override { return ICON_FA_MOUNTAIN " Terrain"; }

	// 描画

	void RenderShadowMap(
		ID3D11DeviceContext* dc,
		const Matrix& lightViewProjection);
	void ApplyDistanceFogSettings(RenderSettings& settings) const;

	// 地形変形

	float GetHeightByUV(float u, float v) const;
	float GetSurfaceHeightByUV(float u, float v) const;
	Vector3 GetSurfaceNormalByUV(float u, float v) const;
	float GetGrassMaskByUV(float u, float v) const;
	int GetSurfaceLayerIndex(const Vector3& worldPosition) const;
	int GetSurfaceLayerIndexByUV(float u, float v) const;
	std::string GetSurfaceLayerName(const Vector3& worldPosition) const;
	int GetTerrainLayerCount() const { return static_cast<int>(terrainLayers.size()); }
	const std::string& GetTerrainLayerName(int index) const { return terrainLayers.at(index).name; }
	void Deform(const Vector3& worldPosition, const Vector3& direction, float power,
		float radius = 0.0f);

	// 地形設定

	float GetTerrainSize() const { return terrainSize; }
	void SetTerrainSize(float value);

	int GetGridResolution() const { return gridResolution; }
	void SetGridResolution(int value);

	float GetTessellationEdgeFactor() const { return tesselation_constant.edge_factor; }
	void SetTessellationEdgeFactor(float value);

	float GetTessellationInnerFactor() const { return tesselation_constant.inner_factor; }
	void SetTessellationInnerFactor(float value);

	float GetHeightScaler() const { return tesselation_constant.height_scaler; }
	void SetHeightScaler(float value);

	float GetTilingScale() const { return tesselation_constant.tilling_scale; }
	void SetTilingScale(float value);

	int GetHeightMapWidth() const { return TerrainTextureWidth; }
	int GetHeightMapHeight() const { return TerrainTextureHeight; }
	ID3D11ShaderResourceView* GetTerrainDataView() const
	{
		return terrainTextureShaderResourceView.Get();
	}

	// コライダー

	uint64_t GetTerrainDataHash() const;
	std::filesystem::path GetColliderVertexPath() const;
	bool BuildGpuColliderMesh(
		float minX,
		float maxX,
		float minZ,
		float maxZ,
		std::vector<Vector3>& vertices,
		std::vector<uint32_t>& indices);
	void BakeCollider();

	// 保存と読み込み

	bool SaveTerrainTexture(const std::string& filename);
	bool LoadTerrainTexture(const std::string& filename);
	bool SaveTerrainMemory(std::vector<uint8_t>& bytes) const;
	bool LoadTerrainMemory(const std::vector<uint8_t>& bytes);
	std::string SaveSettingsJson() const;
	bool LoadSettingsJson(const std::string& text);
	void UseEmbeddedStorage() { terrainFilePath.clear(); }

	// ブラシ

	int GetBrushTextureIndex() const { return currentBrushIndex; }
	bool SetBrushTexture(int index);

	int GetBrushTextureCount() const { return static_cast<int>(brushes.size()); }
	bool AddBrushTexture(const std::string& filename);

private:
	// 定数

	static constexpr int MaxTerrainLayers = 16;
	static constexpr int TerrainTextureWidth = 1024;
	static constexpr int TerrainTextureHeight = 1024;

	// ブラシモード

	enum class BrushMode
	{
		RaiseLower,
		SetHeight,
		Paint,
		GrassPaint,
	};

	// 頂点

	struct TerrainVertex
	{
		Vector3 position;
		Vector3 normal;
		Vector2 texcoord;
	};

	// 定数バッファ

	struct CbShadowMap
	{
		Matrix lightViewProjections[ShadowMapData::CascadeCount];
		Vector4 cascadeSplits;
		Vector4 cameraFront;
		Color shadowColor;
		float shadowBias;
		int pcfKernelSize;
		float dummy[2];
	};

	struct CbMaterial
	{
		Color baseColor;
		Color emissiveColor;
		Color emissionColor;
		Color fresnelColor;

		float metalness;
		float roughness;
		float occlusion;
		float occlusionStrength;

		float shadowStrength;
		float fresnelPower;
		float fresnelStrength;
		int useMetalnessTexture;

		int useRoughnessTexture;
		int useOcclusionTexture;
		int useEmissiveTexture;
		int isFlatShading;

		int useBaseColorTexture;
		int dummy[3];
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
		Color distanceFogColor;
		Vector4 distanceFogParams;
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

	struct CbTerrainLayer
	{
		int layerCount = 0;
		int grassMaskPreview = 0;
		int dummy[2] = {};
	};

	// ブラシ

	struct TerrainBrush
	{
		std::string name;
		std::string filepath;
		int width = 0;
		int height = 0;
		std::vector<float> mask;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderResourceView;
	};

	// 地形レイヤー

	struct TerrainLayer
	{
		std::string name;
		std::string baseColorPath;
		std::string normalPath;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> baseColorView;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> normalView;
	};

	// GPUリソース生成

	void InitializeGpuResources();
	void CreateGridMesh(ID3D11Device* device);
	void CreateTerrainTexture(ID3D11Device* device);
	void UploadTerrainTexture(ID3D11DeviceContext* dc);
	void ClearTerrainTexture();
	bool LoadTerrainImage(
		const DirectX::TexMetadata& sourceMetadata,
		const DirectX::ScratchImage& sourceImage);

	// 地形メッシュ

	void BuildTerrainMesh(
		float minX,
		float maxX,
		float minZ,
		float maxZ,
		std::vector<TerrainVertex>& vertices,
		std::vector<uint32_t>& indices) const;
	void MarkTerrainMeshDirty(bool rebuildGrass = true);

	// 定数バッファ更新

	void UpdateTerrainObjectConstantBuffer(ID3D11DeviceContext* dc);
	void UpdateTerrainSceneConstantBuffer(
		ID3D11DeviceContext* dc,
		const RenderContext& rc);
	void UpdateShadowConstantBuffer(
		ID3D11DeviceContext* dc,
		const std::array<Matrix, ShadowMapData::CascadeCount>& lightViewProjections,
		const Vector4& cascadeSplits,
		const Vector3& cameraFront,
		const Color& shadowColor,
		float shadowBias,
		int pcfKernelSize);
	void UpdateMaterialConstantBuffer(ID3D11DeviceContext* dc);

	// 地形編集

	void PaintByMouse(const RenderContext& rc);
	bool ScreenToTerrainUV(
		const RenderContext& rc,
		float& outU,
		float& outV) const;
	void ApplyBrush(float u, float v, float heightSign);

	// ブラシ処理

	const TerrainBrush* GetCurrentBrush() const;
	float SampleBrushMask(float u, float v) const;
	void DrawBrushGUI();

	// 地形レイヤー処理

	bool AddTerrainLayer(
		const std::string& baseColorPath,
		const std::string& normalPath);
	float GetTerrainLayerValue(int layerIndex) const;
	void DrawTerrainLayerGUI();

	// 地形メッシュ設定

	float terrainSize = 500.0f;
	int gridResolution = 64;
	UINT indexCount = 0;
	CbTessellation tesselation_constant;

	// 地形メッシュ

	Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
	bool terrainMeshDirty = true;

	// 定数バッファ

	Microsoft::WRL::ComPtr<ID3D11Buffer> shadowMapConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> materialConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> terrainObjectConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> terrainSceneConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> tesselationConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> terrainLayerConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> terrainColliderBuildConstantBuffer;

	// シェーダー

	Microsoft::WRL::ComPtr<ID3D11VertexShader> terrainVertexShader;
	Microsoft::WRL::ComPtr<ID3D11HullShader> terrainHullShader;
	Microsoft::WRL::ComPtr<ID3D11DomainShader> terrainDomainShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> terrainPixelShader;
	Microsoft::WRL::ComPtr<ID3D11ComputeShader> terrainColliderBuildComputeShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> terrainInputLayout;

	// 地形テクスチャ

	std::vector<Vector4> terrainPixels;
	bool terrainTextureDirty = true;
	bool is_terrain_texture_clear_color = true;
	Color terrain_texture_clear_color = {0.0f, 0.0f, 0.0f, 1.0f};
	Microsoft::WRL::ComPtr<ID3D11Texture2D> terrainTexture;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> terrainTextureShaderResourceView;

	// 地形レイヤー

	std::vector<TerrainLayer> terrainLayers;
	int currentTerrainLayerIndex = 0;

	// マテリアル

	Color baseColor = {1.0f, 1.0f, 1.0f, 1.0f};
	Color emissiveColor = {0.0f, 0.0f, 0.0f, 1.0f};
	float metalness = 0.0f;
	float roughness = 1.0f;
	float occlusion = 1.0f;
	float occlusionStrength = 1.0f;
	float shadowStrength = 0.85f;

	// 距離フォグ

	bool distanceFogEnabled = true;
	float distanceFogStart = 18.0f;
	float distanceFogEnd = 65.0f;
	float distanceFogStrength = 0.9f;
	Color distanceFogColor = {0.38f, 0.46f, 0.54f, 1.0f};

	// 地形ブラシ

	bool use_brush = false;
	BrushMode brushMode = BrushMode::RaiseLower;
	int brush_size = 32;
	float heightBrushStrength = 0.02f;
	float setHeightValue = 0.0f;
	float paintOpacity = 0.08f;
	bool invertBrushMask = false;
	std::vector<TerrainBrush> brushes;
	int currentBrushIndex = -1;

	// 保存状態

	std::string terrainFilePath;
	std::string terrainIoMessage;
	bool pendingColliderRebuild = false;

	// 草描画
	std::unique_ptr<TerrainGrassRenderer> grassRenderer;
	TerrainGrassRenderer::Settings grassSettings;
	TerrainGrassRenderer::Settings grassDraftSettings;
	bool grassDirty = true;
	bool grassDraftInitialized = false;
	bool grassPaintSessionActive = false;
	bool migrateLegacyGrassMask = false;
	int legacyGrassTerrainLayer = -1;
};
