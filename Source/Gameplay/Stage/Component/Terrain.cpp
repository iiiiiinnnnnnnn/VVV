
#include "IconsFontAwesome5.h"
#include "Gameplay/Stage/Component/Terrain.h"

#include "Gameplay/Actor/Actor.h"
#include "Rendering/Core/Graphics.h"
#include "Resource/GpuResourceUtils.h"
#include "Gameplay/Lighting/LightManager.h"
#include "Application/Input/Input.h"
#include "Application/Time/GameTime.h"
#include "Application/SettingsAndDebug/DebugUtil.h"
#include "Physics/Collider/TerrainMeshCollider.h"
#include "Physics/Navigation/NavMeshActor.h"
#include "Resource/Texture.h"
#include "Resource/ResourceManager.h"
#include "Core/Foundation/Json.h"
#include "Rendering/Effect/Effect.h"
#include "Rendering/Effect/EffectManager.h"
#include <DirectXTex.h>

#include <cfloat>
#include <cmath>

Terrain::Terrain(Object* owner)
	: Component(owner)
{
	InitializeGpuResources();
	grassRenderer = std::make_unique<TerrainGrassRenderer>(Game::Graphics::Instance().GetDevice());
	ClearTerrainTexture();

	AddBrushTexture("Resources/Terrain/Brushes/brush_default.png");
	AddBrushTexture("Resources/Terrain/Brushes/brush_pen.png");
	AddBrushTexture("Resources/Terrain/Brushes/brush_square.png");
	AddBrushTexture("Resources/Terrain/Brushes/brush_triangle.png");
	AddBrushTexture("Resources/Terrain/Brushes/brush_manji.png");
	if (brushes.empty())
	{
		AddBrushTexture("Resources/Image/bugTex.png");
	}

}

void Terrain::SetTerrainSize(float value)
{
	value = std::max(value, 1.0f);
	if (fabsf(terrainSize - value) <= eps) return;

	terrainSize = value;
	MarkTerrainMeshDirty();
}

void Terrain::SetGridResolution(int value)
{
	value = std::clamp(value, 1, 256);
	if (gridResolution == value) return;

	gridResolution = value;
	MarkTerrainMeshDirty();
}

void Terrain::SetTessellationEdgeFactor(float value)
{
	value = std::clamp(value, 1.0f, 16.0f);
	if (fabsf(tesselation_constant.edge_factor - value) <= eps) return;

	tesselation_constant.edge_factor = value;
	MarkTerrainMeshDirty();
}

void Terrain::SetTessellationInnerFactor(float value)
{
	value = std::clamp(value, 1.0f, 16.0f);
	if (fabsf(tesselation_constant.inner_factor - value) <= eps) return;

	tesselation_constant.inner_factor = value;
	MarkTerrainMeshDirty();
}

void Terrain::SetHeightScaler(float value)
{
	value = std::clamp(value, -200.0f, 200.0f);
	if (fabsf(tesselation_constant.height_scaler - value) <= eps) return;

	tesselation_constant.height_scaler = value;
	MarkTerrainMeshDirty();
}

void Terrain::SetTilingScale(float value)
{
	tesselation_constant.tilling_scale = std::clamp(value, 1.0f, 300.0f);
}

void Terrain::InitializeGpuResources()
{
	ID3D11Device* device = Game::Graphics::Instance().GetDevice();

	GpuResourceUtils::CreateConstantBuffer(
		device,
		sizeof(CbShadowMap),
		shadowMapConstantBuffer.GetAddressOf());

	GpuResourceUtils::CreateConstantBuffer(
		device,
		sizeof(CbMaterial),
		materialConstantBuffer.GetAddressOf());

	GpuResourceUtils::CreateConstantBuffer(
		device,
		sizeof(CbTerrainObject),
		terrainObjectConstantBuffer.GetAddressOf());

	GpuResourceUtils::CreateConstantBuffer(
		device,
		sizeof(CbTerrainScene),
		terrainSceneConstantBuffer.GetAddressOf());

	GpuResourceUtils::CreateConstantBuffer(
		device,
		sizeof(CbTessellation),
		tesselationConstantBuffer.GetAddressOf());

	GpuResourceUtils::CreateConstantBuffer(
		device,
		sizeof(CbTerrainLayer),
		terrainLayerConstantBuffer.GetAddressOf());

	GpuResourceUtils::CreateConstantBuffer(
		device,
		sizeof(CbTerrainColliderBuild),
		terrainColliderBuildConstantBuffer.GetAddressOf());

	CreateGridMesh(device);
	CreateTerrainTexture(device);

	D3D11_INPUT_ELEMENT_DESC inputElementDescs[]
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};

	GpuResourceUtils::LoadVertexShader(
		device,
		"Resources/Shader/TerrainPrimitiveMeshVS.cso",
		inputElementDescs,
		_countof(inputElementDescs),
		terrainInputLayout.GetAddressOf(),
		terrainVertexShader.GetAddressOf());

	GpuResourceUtils::LoadHullShader(
		device,
		"Resources/Shader/TerrainPrimitiveHS.cso",
		terrainHullShader.GetAddressOf());

	GpuResourceUtils::LoadDomainShader(
		device,
		"Resources/Shader/TerrainPrimitiveDS.cso",
		terrainDomainShader.GetAddressOf());

	GpuResourceUtils::LoadPixelShader(
		device,
		"Resources/Shader/TerrainPrimitivePS.cso",
		terrainPixelShader.GetAddressOf());

	GpuResourceUtils::LoadComputeShader(
		device,
		"Resources/Shader/TerrainColliderBuildCS.cso",
		terrainColliderBuildComputeShader.GetAddressOf());

	// レイヤー追加
	// ブレンドで違和感のない順番で追加する

	AddTerrainLayer("Resources/Terrain/Layers/stone.png", "Resources/Terrain/Layers/stone_n.png");
	AddTerrainLayer("Resources/Terrain/Layers/rock.png", "Resources/Terrain/Layers/rock_n.png");
	AddTerrainLayer("Resources/Terrain/Layers/dirt.png", "Resources/Terrain/Layers/dirt_n.png");
	AddTerrainLayer("Resources/Terrain/Layers/grass.png", "Resources/Terrain/Layers/grass_n.png");
	AddTerrainLayer("Resources/Terrain/Layers/test.png", "Resources/Terrain/Layers/test.png");

	// エラー用
	if (terrainLayers.empty())
	{
		AddTerrainLayer("Resources/Image/bugTex.png", "Resources/Image/bugTex.png");
	}
}

void Terrain::CreateGridMesh(ID3D11Device* device)
{
	if (gridResolution < 1)
	{
		gridResolution = 1;
	}

	std::vector<TerrainVertex> vertices;
	std::vector<uint32_t> indices;
	BuildTerrainMesh(0.0f, 1.0f, 0.0f, 1.0f, vertices, indices);

	indexCount = static_cast<UINT>(indices.size());

	D3D11_BUFFER_DESC vertexBufferDesc{};
	vertexBufferDesc.ByteWidth = static_cast<UINT>(sizeof(TerrainVertex) * vertices.size());
	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA vertexData{};
	vertexData.pSysMem = vertices.data();

	HRESULT hr = device->CreateBuffer(
		&vertexBufferDesc,
		&vertexData,
		vertexBuffer.ReleaseAndGetAddressOf());

	_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

	D3D11_BUFFER_DESC indexBufferDesc{};
	indexBufferDesc.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * indices.size());
	indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA indexData{};
	indexData.pSysMem = indices.data();

	hr = device->CreateBuffer(
		&indexBufferDesc,
		&indexData,
		indexBuffer.ReleaseAndGetAddressOf());

	_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

	terrainMeshDirty = false;
}

void Terrain::BuildTerrainMesh(
	float minX,
	float maxX,
	float minZ,
	float maxZ,
	std::vector<TerrainVertex>& vertices,
	std::vector<uint32_t>& indices) const
{
	minX = std::clamp(minX, 0.0f, 1.0f);
	maxX = std::clamp(maxX, 0.0f, 1.0f);
	minZ = std::clamp(minZ, 0.0f, 1.0f);
	maxZ = std::clamp(maxZ, 0.0f, 1.0f);

	if (minX > maxX) std::swap(minX, maxX);
	if (minZ > maxZ) std::swap(minZ, maxZ);

	const int meshSegments = std::max(
		gridResolution * std::max(
			static_cast<int>(std::ceil(std::max(
				tesselation_constant.edge_factor,
				tesselation_constant.inner_factor))),
			1),
		1);

	const int minGridX = std::clamp(static_cast<int>(std::floor(minX * meshSegments)), 0, meshSegments - 1);
	const int maxGridX = std::clamp(static_cast<int>(std::ceil(maxX * meshSegments)), minGridX + 1, meshSegments);
	const int minGridZ = std::clamp(static_cast<int>(std::floor(minZ * meshSegments)), 0, meshSegments - 1);
	const int maxGridZ = std::clamp(static_cast<int>(std::ceil(maxZ * meshSegments)), minGridZ + 1, meshSegments);
	const int segmentCountX = maxGridX - minGridX;
	const int segmentCountZ = maxGridZ - minGridZ;
	const int vertexLineCount = segmentCountX + 1;

	vertices.clear();
	indices.clear();
	vertices.reserve(static_cast<size_t>(vertexLineCount) * static_cast<size_t>(segmentCountZ + 1));
	indices.reserve(static_cast<size_t>(segmentCountX) * static_cast<size_t>(segmentCountZ) * 6);

	for (int z = 0; z <= segmentCountZ; ++z)
	{
		for (int x = 0; x <= segmentCountX; ++x)
		{
			const int gridX = minGridX + x;
			const int gridZ = minGridZ + z;
			const float u = static_cast<float>(gridX) / static_cast<float>(meshSegments);
			const float v = static_cast<float>(gridZ) / static_cast<float>(meshSegments);

			TerrainVertex vertex{};
			vertex.position = {
				(u - 0.5f) * terrainSize,
				GetHeightByUV(u, v),
				(v - 0.5f) * terrainSize
			};
			vertex.normal = Vector3::UnitY;
			vertex.texcoord = {u, v};
			vertices.push_back(vertex);
		}
	}

	for (int z = 0; z < segmentCountZ; ++z)
	{
		for (int x = 0; x < segmentCountX; ++x)
		{
			const uint32_t i0 = static_cast<uint32_t>(z * vertexLineCount + x);
			const uint32_t i1 = i0 + 1;
			const uint32_t i2 = i0 + static_cast<uint32_t>(vertexLineCount);
			const uint32_t i3 = i2 + 1;

			indices.push_back(i0);
			indices.push_back(i2);
			indices.push_back(i1);

			indices.push_back(i1);
			indices.push_back(i2);
			indices.push_back(i3);
		}
	}

	for (TerrainVertex& vertex : vertices)
	{
		vertex.normal = Vector3::Zero;
	}

	for (size_t i = 0; i + 2 < indices.size(); i += 3)
	{
		TerrainVertex& v0 = vertices[indices[i + 0]];
		TerrainVertex& v1 = vertices[indices[i + 1]];
		TerrainVertex& v2 = vertices[indices[i + 2]];

		Vector3 normal = (v1.position - v0.position).Cross(v2.position - v0.position);
		if (normal.LengthSquared() > eps)
		{
			normal.Normalize();
			v0.normal += normal;
			v1.normal += normal;
			v2.normal += normal;
		}
	}

	for (TerrainVertex& vertex : vertices)
	{
		if (vertex.normal.LengthSquared() > eps)
			vertex.normal.Normalize();
		else
			vertex.normal = Vector3::UnitY;
	}
}

void Terrain::MarkTerrainMeshDirty()
{
	terrainMeshDirty = true;
	pendingColliderRebuild = true;
}

void Terrain::CreateTerrainTexture(ID3D11Device* device)
{
	terrainPixels.resize(TerrainTextureWidth * TerrainTextureHeight);

	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = TerrainTextureWidth;
	desc.Height = TerrainTextureHeight;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA data{};
	data.pSysMem = terrainPixels.data();
	data.SysMemPitch = sizeof(Vector4) * TerrainTextureWidth;

	HRESULT hr = device->CreateTexture2D(
		&desc,
		&data,
		terrainTexture.GetAddressOf());

	_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

	hr = device->CreateShaderResourceView(
		terrainTexture.Get(),
		nullptr,
		terrainTextureShaderResourceView.GetAddressOf());

	_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
}

void Terrain::UploadTerrainTexture(ID3D11DeviceContext* dc)
{
	if (!terrainTextureDirty)
	{
		return;
	}

	dc->UpdateSubresource(
		terrainTexture.Get(),
		0,
		nullptr,
		terrainPixels.data(),
		sizeof(Vector4) * TerrainTextureWidth,
		0);

	terrainTextureDirty = false;
}

void Terrain::ClearTerrainTexture()
{
	navigationBaseHeights.clear();
	navMeshMaskInitialized = true;
	for (Vector4& pixel : terrainPixels)
	{
		pixel.x = terrain_texture_clear_color.x;
		pixel.y = terrain_texture_clear_color.y;
		pixel.z = terrain_texture_clear_color.z;
		pixel.w = terrain_texture_clear_color.w;
	}

	terrainTextureDirty = true;

	terrainEditHashDirty = true;
	is_terrain_texture_clear_color = false;
	MarkTerrainMeshDirty();
}

void Terrain::UpdateTerrainObjectConstantBuffer(ID3D11DeviceContext* dc)
{
	Transform* transform = owner->GetComponent<Transform>();
	if (!transform) return;

	CbTerrainObject cbObject{};
	cbObject.world = transform->matrix;
	cbObject.terrainSize = terrainSize;
	cbObject.heightMapTexelSize = 1.0f / static_cast<float>(TerrainTextureWidth);

	dc->UpdateSubresource(
		terrainObjectConstantBuffer.Get(),
		0,
		nullptr,
		&cbObject,
		0,
		0);
}

void Terrain::UpdateTerrainSceneConstantBuffer(
	ID3D11DeviceContext* dc,
	const RenderContext& rc)
{
	CbTerrainScene cbScene{};
	cbScene.viewProjection = rc.camera->GetView() * rc.camera->GetProjection();
	cbScene.viewPosition = rc.camera->GetEye();
	cbScene.lightData = rc.lightManager->ConvertToCb();
	cbScene.distanceFogColor = rc.renderSettings.distanceFogColor;
	cbScene.distanceFogParams = {
		rc.renderSettings.distanceFogStart,
		rc.renderSettings.distanceFogEnd,
		rc.renderSettings.distanceFogStrength,
		rc.renderSettings.distanceFogEnabled ? 1.0f : 0.0f};
	dc->UpdateSubresource(
		terrainSceneConstantBuffer.Get(),
		0,
		nullptr,
		&cbScene,
		0,
		0);
}

void Terrain::UpdateShadowConstantBuffer(
	ID3D11DeviceContext* dc,
	const std::array<Matrix, ShadowMapData::CascadeCount>& lightViewProjections,
	const Vector4& cascadeSplits,
	const Vector3& cameraFront,
	const Color& shadowColor,
	float shadowBias,
	int pcfKernelSize)
{
	CbShadowMap cbShadow{};
	for (int cascadeIndex = 0;
		 cascadeIndex < ShadowMapData::CascadeCount;
		 ++cascadeIndex)
	{
		cbShadow.lightViewProjections[cascadeIndex] =
			lightViewProjections[cascadeIndex];
	}
	cbShadow.cascadeSplits = cascadeSplits;
	cbShadow.cameraFront = Vector4(
		cameraFront.x,
		cameraFront.y,
		cameraFront.z,
		0.0f);
	cbShadow.shadowColor = shadowColor;
	cbShadow.shadowBias = shadowBias;
	cbShadow.pcfKernelSize = pcfKernelSize;

	dc->UpdateSubresource(
		shadowMapConstantBuffer.Get(),
		0,
		nullptr,
		&cbShadow,
		0,
		0);
}

void Terrain::UpdateMaterialConstantBuffer(ID3D11DeviceContext* dc)
{
	CbMaterial cbMaterial{};
	cbMaterial.baseColor = baseColor;
	cbMaterial.emissiveColor = emissiveColor;
	cbMaterial.emissionColor = Color(0.0f, 0.0f, 0.0f, 0.0f);
	cbMaterial.fresnelColor = Color(1.0f, 1.0f, 1.0f, 0.0f);
	cbMaterial.fresnelPower = 3.0f;
	cbMaterial.fresnelStrength = 0.0f;
	cbMaterial.metalness = std::clamp(metalness, 0.0f, 1.0f);
	cbMaterial.roughness = std::clamp(roughness, 0.0001f, 1.0f);
	cbMaterial.occlusion = std::clamp(occlusion, 0.0f, 1.0f);
	cbMaterial.occlusionStrength = std::clamp(occlusionStrength, 0.0f, 1.0f);
	cbMaterial.shadowStrength = std::clamp(shadowStrength, 0.0f, 1.0f);
	cbMaterial.useMetalnessTexture = 0;
	cbMaterial.useRoughnessTexture = 0;
	cbMaterial.useOcclusionTexture = 0;
	cbMaterial.useEmissiveTexture = 0;
	cbMaterial.isFlatShading = 0;
	cbMaterial.useBaseColorTexture = 0;

	dc->UpdateSubresource(
		materialConstantBuffer.Get(),
		0,
		nullptr,
		&cbMaterial,
		0,
		0);
}

void Terrain::Update()
{
	grassRemovalEffectCooldown = std::max(
		grassRemovalEffectCooldown - Game::Time::deltaTime, 0.0f);
}

void Terrain::Render(const RenderContext& rc)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	if (is_terrain_texture_clear_color)
	{
		ClearTerrainTexture();
	}

	PaintByMouse(rc);
	UploadTerrainTexture(dc);
	if (terrainMeshDirty)
	{
		CreateGridMesh(Game::Graphics::Instance().GetDevice());
	}

	UpdateTerrainObjectConstantBuffer(dc);
	UpdateTerrainSceneConstantBuffer(dc, rc);
	UpdateShadowConstantBuffer(
		dc,
		rc.shadowMapData.lightViewProjections,
		rc.shadowMapData.cascadeSplits,
		rc.camera->GetFront(),
		rc.shadowMapData.shadowColor,
		rc.shadowMapData.shadowBias,
		rc.shadowMapData.pcfKernelSize);
	UpdateMaterialConstantBuffer(dc);

	dc->UpdateSubresource(
		tesselationConstantBuffer.Get(),
		0,
		nullptr,
		&tesselation_constant,
		0,
		0);

	CbTerrainLayer cbTerrainLayer{};
	cbTerrainLayer.layerCount = static_cast<int>(terrainLayers.size());
	if (use_brush)
	{
		cbTerrainLayer.brushPreviewMode = static_cast<int>(brushMode) + 1;
		cbTerrainLayer.brushFlags = (invertBrushMask ? 1 : 0) |
			(ImGui::GetIO().KeyShift && brushMode != BrushMode::SetHeight && brushMode != BrushMode::Paint ? 2 : 0);
		cbTerrainLayer.brushParams = Vector4(heightBrushStrength, setHeightValue,
			GetTerrainLayerValue(currentTerrainLayerIndex), paintOpacity);
		const TerrainBrush* brush = GetCurrentBrush();
		if (brush) cbTerrainLayer.brushUsesAlpha = brush->usesAlpha;
		float u = 0.0f;
		float v = 0.0f;
		if (brush && (useBrushViewportInput ? brushViewportInputAllowed : !ImGui::GetIO().WantCaptureMouse) &&
			Game::Input::IsFocusedWindow() && ScreenToTerrainUV(rc, u, v))
		{
			const int x = std::clamp(static_cast<int>(u * TerrainTextureWidth), 0, TerrainTextureWidth - 1);
			const int y = std::clamp(static_cast<int>(v * TerrainTextureHeight), 0, TerrainTextureHeight - 1);
			cbTerrainLayer.brushArea = Vector4((x + 0.5f) / TerrainTextureWidth,
				(y + 0.5f) / TerrainTextureHeight, static_cast<float>(brush_size) / TerrainTextureWidth, 1.0f);
		}
	}
	dc->UpdateSubresource(
		terrainLayerConstantBuffer.Get(),
		0,
		nullptr,
		&cbTerrainLayer,
		0,
		0);

	dc->OMSetBlendState(
		rc.renderState->GetBlendState(BlendState::Opaque),
		nullptr,
		0xFFFFFFFF);

	dc->OMSetDepthStencilState(
		rc.renderState->GetDepthStencilState(DepthState::TestAndWrite),
		0);

	dc->RSSetState(
		rc.renderState->GetRasterizerState(RasterizerState::SolidCullNone));

	UINT stride = sizeof(TerrainVertex);
	UINT offset = 0;
	ID3D11Buffer* vertexBuffers[] = {vertexBuffer.Get()};

	dc->IASetInputLayout(terrainInputLayout.Get());
	dc->IASetVertexBuffers(0, 1, vertexBuffers, &stride, &offset);
	dc->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	dc->VSSetShader(terrainVertexShader.Get(), nullptr, 0);
	dc->HSSetShader(nullptr, nullptr, 0);
	dc->DSSetShader(nullptr, nullptr, 0);
	dc->PSSetShader(terrainPixelShader.Get(), nullptr, 0);

	ID3D11Buffer* objectCb = terrainObjectConstantBuffer.Get();
	ID3D11Buffer* shadowCb = shadowMapConstantBuffer.Get();
	ID3D11Buffer* sceneCb = terrainSceneConstantBuffer.Get();
	ID3D11Buffer* layerCb = terrainLayerConstantBuffer.Get();

	ID3D11Buffer* psPbrCbs[] =
	{
		shadowMapConstantBuffer.Get(),
		materialConstantBuffer.Get(),
		tesselationConstantBuffer.Get(),
	};

	dc->VSSetConstantBuffers(0, 1, &shadowCb);
	dc->VSSetConstantBuffers(3, 1, &objectCb);
	dc->VSSetConstantBuffers(7, 1, &sceneCb);
	dc->PSSetConstantBuffers(0, _countof(psPbrCbs), psPbrCbs);
	dc->PSSetConstantBuffers(4, 1, &layerCb);
	dc->PSSetConstantBuffers(7, 1, &sceneCb);

	ID3D11ShaderResourceView* terrainSrv = terrainTextureShaderResourceView.Get();
	dc->PSSetShaderResources(0, 1, &terrainSrv);
	const TerrainBrush* previewBrush = GetCurrentBrush();
	ID3D11ShaderResourceView* brushSrv = previewBrush ? previewBrush->shaderResourceView.Get() : nullptr;
	dc->PSSetShaderResources(1, 1, &brushSrv);

	dc->PSSetShaderResources(
		8,
		ShadowMapData::CascadeCount,
		rc.shadowMapData.shadowMaps.data());

	ID3D11ShaderResourceView* iblSrvs[] =
	{
		rc.iblData.ggxLookUpTableMap,
		rc.iblData.specularPremappingRadianceEnvironmentMap,
		rc.iblData.diffuseIrradianceEnvironmentMap,
	};
	dc->PSSetShaderResources(17, _countof(iblSrvs), iblSrvs);

	ID3D11ShaderResourceView* terrainBaseColorSrvs[MaxTerrainLayers] = {};
	ID3D11ShaderResourceView* terrainNormalSrvs[MaxTerrainLayers] = {};
	for (int i = 0; i < static_cast<int>(terrainLayers.size()) && i < MaxTerrainLayers; ++i)
	{
		terrainBaseColorSrvs[i] = terrainLayers[i].baseColorView.Get();
		terrainNormalSrvs[i] = terrainLayers[i].normalView.Get();
	}

	dc->PSSetShaderResources(20, MaxTerrainLayers, terrainBaseColorSrvs);
	dc->PSSetShaderResources(36, MaxTerrainLayers, terrainNormalSrvs);

	ID3D11SamplerState* samplerStates[] =
	{
		rc.renderState->GetSamplerState(SamplerState::PointClamp),
		rc.renderState->GetSamplerState(SamplerState::LinearClamp),
		rc.renderState->GetSamplerState(SamplerState::AnisotropicWrap),
	};

	dc->PSSetSamplers(0, _countof(samplerStates), samplerStates);

	dc->DrawIndexed(indexCount, 0, 0);

	ID3D11Buffer* nullCb = nullptr;
	ID3D11Buffer* nullCbs3[] = {nullptr, nullptr, nullptr};

	dc->VSSetConstantBuffers(0, 1, &nullCb);
	dc->VSSetConstantBuffers(3, 1, &nullCb);
	dc->VSSetConstantBuffers(7, 1, &nullCb);
	dc->PSSetConstantBuffers(0, _countof(nullCbs3), nullCbs3);
	dc->PSSetConstantBuffers(4, 1, &nullCb);
	dc->PSSetConstantBuffers(7, 1, &nullCb);

	ID3D11ShaderResourceView* nullSrv = nullptr;
	ID3D11ShaderResourceView* nullSrvs3[] = {nullptr, nullptr, nullptr};
	ID3D11ShaderResourceView* nullTerrainSrvs[MaxTerrainLayers] = {};

	dc->PSSetShaderResources(0, 1, &nullSrv);
	dc->PSSetShaderResources(1, 1, &nullSrv);
	ID3D11ShaderResourceView* nullShadowSrvs[ShadowMapData::CascadeCount] = {};
	dc->PSSetShaderResources(8, ShadowMapData::CascadeCount, nullShadowSrvs);
	dc->PSSetShaderResources(17, _countof(nullSrvs3), nullSrvs3);
	dc->PSSetShaderResources(20, MaxTerrainLayers, nullTerrainSrvs);
	dc->PSSetShaderResources(36, MaxTerrainLayers, nullTerrainSrvs);

	ID3D11SamplerState* nullSamplers3[] = {nullptr, nullptr, nullptr};
	dc->PSSetSamplers(0, _countof(nullSamplers3), nullSamplers3);

	dc->VSSetShader(nullptr, nullptr, 0);
	dc->PSSetShader(nullptr, nullptr, 0);
	dc->IASetInputLayout(nullptr);

	const bool paintingGrass = use_brush && brushMode == BrushMode::GrassPaint;
	if (grassRenderer && grassDirty && !paintingGrass)
	{
		grassRenderer->Rebuild(*this, grassSettings);
		grassDirty = false;
	}
	if (rc.renderSettings.showGrass && grassRenderer && !paintingGrass)
	{
		Matrix terrainWorld = Matrix::Identity;
		if (owner)
		{
			const Transform* transform = owner->GetTransform();
			if (transform)
			{
				terrainWorld = transform->matrix;
			}
		}
		grassRenderer->Render(rc, terrainWorld, grassSettings,
			GetTerrainDataView(), terrainSize);
	}
}

void Terrain::RenderShadowMap(
	ID3D11DeviceContext* dc,
	const Matrix& lightViewProjection)
{
	if (is_terrain_texture_clear_color)
	{
		ClearTerrainTexture();
	}

	UploadTerrainTexture(dc);
	if (terrainMeshDirty)
	{
		CreateGridMesh(Game::Graphics::Instance().GetDevice());
	}
	UpdateTerrainObjectConstantBuffer(dc);
	UpdateShadowConstantBuffer(
		dc,
		std::array<Matrix, ShadowMapData::CascadeCount>{
			lightViewProjection,
			lightViewProjection,
			lightViewProjection,
			lightViewProjection},
		Vector4(FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX),
		Vector3::Forward,
		Color(0.0f, 0.0f, 0.0f, 1.0f),
		0.0f,
		1);

	CbTerrainScene cbScene{};
	cbScene.viewProjection = lightViewProjection;

	dc->UpdateSubresource(
		terrainSceneConstantBuffer.Get(),
		0,
		nullptr,
		&cbScene,
		0,
		0);

	dc->UpdateSubresource(
		tesselationConstantBuffer.Get(),
		0,
		nullptr,
		&tesselation_constant,
		0,
		0);

	UINT stride = sizeof(TerrainVertex);
	UINT offset = 0;
	ID3D11Buffer* vertexBuffers[] = {vertexBuffer.Get()};

	dc->IASetInputLayout(terrainInputLayout.Get());
	dc->IASetVertexBuffers(0, 1, vertexBuffers, &stride, &offset);
	dc->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	dc->VSSetShader(terrainVertexShader.Get(), nullptr, 0);
	dc->HSSetShader(nullptr, nullptr, 0);
	dc->DSSetShader(nullptr, nullptr, 0);
	dc->PSSetShader(nullptr, nullptr, 0);

	ID3D11Buffer* objectCb = terrainObjectConstantBuffer.Get();
	ID3D11Buffer* shadowCb = shadowMapConstantBuffer.Get();
	ID3D11Buffer* sceneCb = terrainSceneConstantBuffer.Get();

	dc->VSSetConstantBuffers(0, 1, &shadowCb);
	dc->VSSetConstantBuffers(3, 1, &objectCb);
	dc->VSSetConstantBuffers(7, 1, &sceneCb);

	dc->DrawIndexed(indexCount, 0, 0);

	ID3D11Buffer* nullCb = nullptr;

	dc->VSSetConstantBuffers(0, 1, &nullCb);
	dc->VSSetConstantBuffers(3, 1, &nullCb);
	dc->VSSetConstantBuffers(7, 1, &nullCb);

	dc->VSSetShader(nullptr, nullptr, 0);
	dc->IASetInputLayout(nullptr);
}

void Terrain::PaintByMouse(const RenderContext& rc)
{
	auto& io = ImGui::GetIO();
	if (!use_brush)
	{
		return;
	}

	if (!Game::Input::IsFocusedWindow())
	{
		return;
	}

	if (io.KeyAlt)
	{
		return;
	}

	if (!io.WantTextInput)
	{
		// [] でブラシサイズ変更
		const int step = std::max(brush_size / 10, 1);
		if (ImGui::IsKeyPressed(ImGuiKey_RightBracket, true)) brush_size = std::max(brush_size - step, 1);
		// なぜか]ではなく@になるので色々さがして変えました
		if (ImGui::IsKeyPressed(ImGuiKey_Backslash, true)) brush_size = std::min(brush_size + step, 256);
	}
	if (useBrushViewportInput ? !brushViewportInputAllowed : io.WantCaptureMouse) return;
	Mouse& mouse = Game::Input::Instance().GetMouse();

	if ((mouse.GetButton() & Mouse::BTN_LEFT) == 0)
	{
		return;
	}

	float u = 0.0f;
	float v = 0.0f;
	if (!ScreenToTerrainUV(rc, u, v))
	{
		return;
	}

	float heightSign = 1.0f;
	if (::GetAsyncKeyState(VK_SHIFT) & 0x8000)
	{
		heightSign = -1.0f;
	}

	ApplyBrush(u, v, heightSign);
}

bool Terrain::ScreenToTerrainUV(const RenderContext& rc, float& outU, float& outV) const
{
	Mouse& mouse = Game::Input::Instance().GetMouse();

	float screenWidth = Game::Graphics::ScreenWidth;
	float screenHeight = Game::Graphics::ScreenHeight;

	if (screenWidth <= 0.0f || screenHeight <= 0.0f)
	{
		return false;
	}

	const Vector2 mouseNdc = Game::Graphics::Instance().GetMouseNDC(
		static_cast<float>(mouse.GetPositionX()),
		static_cast<float>(mouse.GetPositionY()));

	Matrix viewProjection = rc.camera->GetView() * rc.camera->GetProjection();
	Matrix invViewProjection = viewProjection.Invert();

	Vector3 nearPoint =
		Vector3::Transform(Vector3(mouseNdc.x, mouseNdc.y, 0.0f), invViewProjection);
	Vector3 farPoint =
		Vector3::Transform(Vector3(mouseNdc.x, mouseNdc.y, 1.0f), invViewProjection);

	Vector3 rayOriginWorld = nearPoint;
	Vector3 rayDirectionWorld = farPoint - nearPoint;
	rayDirectionWorld.Normalize();

	Transform* transform = owner->GetComponent<Transform>();
	if (!transform) return false;
	Matrix invWorld = transform->matrix.Invert();

	Vector3 rayOriginLocal = Vector3::Transform(rayOriginWorld, invWorld);
	Vector3 rayDirectionLocal = Vector3::TransformNormal(rayDirectionWorld, invWorld);
	rayDirectionLocal.Normalize();

	if (terrainSize <= 0.0f || terrainPixels.empty()) return false;
	const Vector3 farLocal = Vector3::Transform(farPoint, invWorld);
	float begin = 0.0f;
	float end = (farLocal - rayOriginLocal).Length();
	const float halfSize = terrainSize * 0.5f;
	auto clipAxis = [&](float origin, float direction) {
		if (fabsf(direction) < eps) return origin >= -halfSize && origin <= halfSize;
		float entry = (-halfSize - origin) / direction;
		float exit = (halfSize - origin) / direction;
		if (entry > exit) std::swap(entry, exit);
		begin = std::max(begin, entry);
		end = std::min(end, exit);
		return begin <= end;
	};
	if (!clipAxis(rayOriginLocal.x, rayDirectionLocal.x) ||
		!clipAxis(rayOriginLocal.z, rayDirectionLocal.z)) return false;

	const int factor = std::max(static_cast<int>(std::ceil(std::max(
		tesselation_constant.edge_factor, tesselation_constant.inner_factor))), 1);
	const int segments = std::max(gridResolution * factor, 1);
	const float cellSize = terrainSize / segments;
	const Vector3 entry = rayOriginLocal + rayDirectionLocal * begin;
	int x = std::clamp(static_cast<int>(floorf((entry.x + halfSize) / cellSize)), 0, segments - 1);
	int z = std::clamp(static_cast<int>(floorf((entry.z + halfSize) / cellSize)), 0, segments - 1);
	const int stepX = rayDirectionLocal.x > 0.0f ? 1 : -1;
	const int stepZ = rayDirectionLocal.z > 0.0f ? 1 : -1;
	auto edgeTime = [&](int cell, int step, float origin, float direction) {
		if (fabsf(direction) < eps) return end + 1.0f;
		return ((cell + (step > 0 ? 1 : 0)) * cellSize - halfSize - origin) / direction;
	};
	auto intersectTriangle = [&](const Vector3& a, const Vector3& b, const Vector3& c, float& hitTime) {
		const Vector3 edge1 = b - a;
		const Vector3 edge2 = c - a;
		const Vector3 cross = rayDirectionLocal.Cross(edge2);
		const float determinant = edge1.Dot(cross);
		if (fabsf(determinant) < eps) return false;
		const Vector3 offset = rayOriginLocal - a;
		const float u = offset.Dot(cross) / determinant;
		if (u < 0.0f || u > 1.0f) return false;
		const Vector3 q = offset.Cross(edge1);
		const float v = rayDirectionLocal.Dot(q) / determinant;
		if (v < 0.0f || u + v > 1.0f) return false;
		hitTime = edge2.Dot(q) / determinant;
		return hitTime >= begin - 0.0001f && hitTime <= end;
	};
	// レイが通るセルを手前から調べ、高さ0の平面ではなく地形表面へ当てる。
	while (x >= 0 && x < segments && z >= 0 && z < segments && begin <= end)
	{
		const float u0 = static_cast<float>(x) / segments;
		const float u1 = static_cast<float>(x + 1) / segments;
		const float v0 = static_cast<float>(z) / segments;
		const float v1 = static_cast<float>(z + 1) / segments;
		const Vector3 p00((u0 - 0.5f) * terrainSize, GetHeightByUV(u0, v0), (v0 - 0.5f) * terrainSize);
		const Vector3 p10((u1 - 0.5f) * terrainSize, GetHeightByUV(u1, v0), (v0 - 0.5f) * terrainSize);
		const Vector3 p01((u0 - 0.5f) * terrainSize, GetHeightByUV(u0, v1), (v1 - 0.5f) * terrainSize);
		const Vector3 p11((u1 - 0.5f) * terrainSize, GetHeightByUV(u1, v1), (v1 - 0.5f) * terrainSize);
		float first = end;
		float second = end;
		const bool hitFirst = intersectTriangle(p00, p01, p10, first);
		const bool hitSecond = intersectTriangle(p10, p01, p11, second);
		if (hitFirst || hitSecond)
		{
			const float hitTime = hitFirst && hitSecond ? std::min(first, second) : hitFirst ? first : second;
			const Vector3 hit = rayOriginLocal + rayDirectionLocal * hitTime;
			outU = std::clamp(hit.x / terrainSize + 0.5f, 0.0f, 1.0f);
			outV = std::clamp(hit.z / terrainSize + 0.5f, 0.0f, 1.0f);
			return true;
		}
		const float nextX = edgeTime(x, stepX, rayOriginLocal.x, rayDirectionLocal.x);
		const float nextZ = edgeTime(z, stepZ, rayOriginLocal.z, rayDirectionLocal.z);
		begin = std::min(nextX, nextZ);
		if (nextX <= nextZ) x += stepX;
		if (nextZ <= nextX) z += stepZ;
	}
	return false;
}

void Terrain::ApplyBrush(float u, float v, float heightSign)
{
	const TerrainBrush* brush = GetCurrentBrush();
	if (brush == nullptr || brush_size <= 0)
	{
		return;
	}

	const int centerX = std::clamp(static_cast<int>(u * TerrainTextureWidth), 0, TerrainTextureWidth - 1);
	const int centerY = std::clamp(static_cast<int>(v * TerrainTextureHeight), 0, TerrainTextureHeight - 1);
	const int radius = brush_size;

	const int x0 = std::max(centerX - radius, 0);
	const int y0 = std::max(centerY - radius, 0);
	const int x1 = std::min(centerX + radius, TerrainTextureWidth - 1);
	const int y1 = std::min(centerY + radius, TerrainTextureHeight - 1);
	const float brushDiameter = static_cast<float>(radius * 2);

	if (brushDiameter <= 0.0f)
	{
		return;
	}

	for (int y = y0; y <= y1; ++y)
	{
		for (int x = x0; x <= x1; ++x)
		{
			const float brushU =
				static_cast<float>(x - (centerX - radius)) / brushDiameter;

			const float brushV =
				static_cast<float>(y - (centerY - radius)) / brushDiameter;

			float mask = SampleBrushMask(brushU, brushV);
			if (invertBrushMask)
			{
				mask = 1.0f - mask;
			}

			if (mask <= 0.0001f)
			{
				continue;
			}

			Vector4& pixel = terrainPixels[
				static_cast<size_t>(y) * TerrainTextureWidth +
				static_cast<size_t>(x)];

			if (brushMode == BrushMode::RaiseLower)
			{
				pixel.x += heightBrushStrength * heightSign * mask;
			}
			else if (brushMode == BrushMode::SetHeight)
			{
				const float setAmount = std::clamp(mask, 0.0f, 1.0f);
				pixel.x += (setHeightValue - pixel.x) * setAmount;
			}
			else if (brushMode == BrushMode::Paint)
			{
				const float paintAmount = std::clamp(
					paintOpacity * mask,
					0.0f,
					1.0f);
				const float targetLayer = GetTerrainLayerValue(currentTerrainLayerIndex);

				pixel.y += (targetLayer - pixel.y) * paintAmount;
				pixel.y = std::clamp(pixel.y, 0.0f, 1.0f);
			}
			else if (brushMode == BrushMode::GrassPaint)
			{
				const float paintAmount = std::clamp(paintOpacity * mask, 0.0f, 1.0f);
				float targetGrass = 1.0f;
				if (heightSign < 0.0f) targetGrass = 0.0f;
				pixel.z += (targetGrass - pixel.z) * paintAmount;
				pixel.z = std::clamp(pixel.z, 0.0f, 1.0f);
			}
			else if (brushMode == BrushMode::NavMeshPaint)
			{
				const float amount = std::clamp(paintOpacity * mask, 0.0f, 1.0f);
				pixel.w = std::clamp(pixel.w + ((heightSign < 0.0f ? 0.0f : 1.0f) - pixel.w) * amount, 0.0f, 1.0f);
				navMeshMaskInitialized = true;
			}
		}
	}

	terrainTextureDirty = true;

	terrainEditHashDirty = true;
	const bool heightChanged = brushMode == BrushMode::RaiseLower || brushMode == BrushMode::SetHeight;
	if (heightChanged)
	{
		navigationBaseHeights.clear();
		MarkTerrainMeshDirty();
	}
	if (brushMode == BrushMode::GrassPaint) grassDirty = true;
	if (heightChanged || brushMode == BrushMode::NavMeshPaint)
	{
		if (NavMeshActor* navMesh = owner->GetComponent<NavMeshActor>())
		{
			const Vector3 center((u - 0.5f) * terrainSize, GetHeightByUV(u, v), (v - 0.5f) * terrainSize);
			navMesh->RequestBuildRegion(center, (brush_size + 1.0f) / TerrainTextureWidth * terrainSize);
		}
	}
}

bool Terrain::AddBrushTexture(const std::string& filename)
{
	if (filename.empty())
	{
		terrainIoMessage = "Brush add failed: filename is empty.";
		return false;
	}

	const std::filesystem::path filepath =
		std::filesystem::path(filename).lexically_normal();

	if (!std::filesystem::exists(filepath))
	{
		terrainIoMessage = "Brush file not found: " + filepath.generic_string();
		return false;
	}

	const std::string normalizedPath = filepath.generic_string();

	for (int i = 0; i < static_cast<int>(brushes.size()); ++i)
	{
		if (brushes[i].filepath == normalizedPath)
		{
			currentBrushIndex = i;
			return true;
		}
	}

	DirectX::TexMetadata sourceMetadata{};
	DirectX::ScratchImage sourceImage;

	HRESULT hr = GpuResourceUtils::LoadImageFile(filepath, sourceMetadata, sourceImage);
	if (FAILED(hr))
	{
		terrainIoMessage = "Brush image load failed: " + normalizedPath;
		return false;
	}

	const DirectX::ScratchImage* workingImage = &sourceImage;
	DirectX::ScratchImage convertedImage;

	if (sourceMetadata.format != DXGI_FORMAT_R8G8B8A8_UNORM)
	{
		if (DirectX::IsCompressed(sourceMetadata.format))
		{
			hr = DirectX::Decompress(
				sourceImage.GetImages(),
				sourceImage.GetImageCount(),
				sourceMetadata,
				DXGI_FORMAT_R8G8B8A8_UNORM,
				convertedImage);
		}
		else
		{
			hr = DirectX::Convert(
				sourceImage.GetImages(),
				sourceImage.GetImageCount(),
				sourceMetadata,
				DXGI_FORMAT_R8G8B8A8_UNORM,
				DirectX::TEX_FILTER_DEFAULT,
				DirectX::TEX_THRESHOLD_DEFAULT,
				convertedImage);
		}

		if (FAILED(hr))
		{
			terrainIoMessage = "Brush format conversion failed: " + normalizedPath;
			return false;
		}

		workingImage = &convertedImage;
	}

	const DirectX::Image* image = workingImage->GetImage(0, 0, 0);
	if (image == nullptr)
	{
		terrainIoMessage = "Brush image data is empty: " + normalizedPath;
		return false;
	}

	TerrainBrush brush;
	brush.name = filepath.stem().string();
	brush.filepath = normalizedPath;
	brush.width = static_cast<int>(image->width);
	brush.height = static_cast<int>(image->height);

	if (brush.width <= 0 || brush.height <= 0)
	{
		terrainIoMessage = "Brush image size is invalid: " + normalizedPath;
		return false;
	}

	brush.mask.resize(
		static_cast<size_t>(brush.width) *
		static_cast<size_t>(brush.height));

	uint8_t minimumAlpha = 255;
	uint8_t maximumAlpha = 0;

	for (int y = 0; y < brush.height; ++y)
	{
		const uint8_t* row = image->pixels + static_cast<size_t>(y) * image->rowPitch;

		for (int x = 0; x < brush.width; ++x)
		{
			const uint8_t* pixel = row + static_cast<size_t>(x) * 4;
			minimumAlpha = std::min(minimumAlpha, pixel[3]);
			maximumAlpha = std::max(maximumAlpha, pixel[3]);
		}
	}

	const bool useAlphaMask =
		static_cast<int>(maximumAlpha) -
		static_cast<int>(minimumAlpha) > 4;

	brush.usesAlpha = useAlphaMask;
	for (int y = 0; y < brush.height; ++y)
	{
		const uint8_t* row = image->pixels + static_cast<size_t>(y) * image->rowPitch;

		for (int x = 0; x < brush.width; ++x)
		{
			const uint8_t* pixel = row + static_cast<size_t>(x) * 4;

			const float red = static_cast<float>(pixel[0]) / 255.0f;
			const float green = static_cast<float>(pixel[1]) / 255.0f;
			const float blue = static_cast<float>(pixel[2]) / 255.0f;
			const float alpha = static_cast<float>(pixel[3]) / 255.0f;

			const float luminance =
				red * 0.2126f +
				green * 0.7152f +
				blue * 0.0722f;

			brush.mask[
				static_cast<size_t>(y) * static_cast<size_t>(brush.width) +
				static_cast<size_t>(x)] =
				std::clamp(useAlphaMask ? alpha : luminance, 0.0f, 1.0f);
		}
	}

	hr = DirectX::CreateShaderResourceView(
		Game::Graphics::Instance().GetDevice(),
		workingImage->GetImages(),
		workingImage->GetImageCount(),
		workingImage->GetMetadata(),
		brush.shaderResourceView.GetAddressOf());

	if (FAILED(hr))
	{
		terrainIoMessage = "Brush GPU texture creation failed: " + normalizedPath;
		return false;
	}

	brushes.push_back(std::move(brush));

	if (currentBrushIndex < 0)
	{
		currentBrushIndex = 0;
	}

	terrainIoMessage = "Brush added: " + brushes.back().name;
	return true;
}

bool Terrain::SetBrushTexture(int index)
{
	if (index < 0 || index >= static_cast<int>(brushes.size()))
	{
		terrainIoMessage = "Brush switch failed: invalid index.";
		return false;
	}

	currentBrushIndex = index;
	terrainIoMessage = "Brush selected: " + brushes[currentBrushIndex].name;
	return true;
}

const Terrain::TerrainBrush* Terrain::GetCurrentBrush() const
{
	if (currentBrushIndex < 0 || currentBrushIndex >= static_cast<int>(brushes.size()))
	{
		return nullptr;
	}

	return &brushes[currentBrushIndex];
}

float Terrain::SampleBrushMask(float u, float v) const
{
	const TerrainBrush* brush = GetCurrentBrush();

	if (brush == nullptr || brush->mask.empty() || brush->width <= 0 || brush->height <= 0)
	{
		return 0.0f;
	}

	if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f)
	{
		return 0.0f;
	}

	const float sourceX = u * static_cast<float>(brush->width - 1);
	const float sourceY = v * static_cast<float>(brush->height - 1);

	const int x0 = static_cast<int>(std::floor(sourceX));
	const int y0 = static_cast<int>(std::floor(sourceY));
	const int x1 = std::min(x0 + 1, brush->width - 1);
	const int y1 = std::min(y0 + 1, brush->height - 1);

	const float tx = sourceX - static_cast<float>(x0);
	const float ty = sourceY - static_cast<float>(y0);

	auto GetMask = [brush](int x, int y)
	{
		return brush->mask[
			static_cast<size_t>(y) * static_cast<size_t>(brush->width) +
			static_cast<size_t>(x)];
	};

	const float mask00 = GetMask(x0, y0);
	const float mask10 = GetMask(x1, y0);
	const float mask01 = GetMask(x0, y1);
	const float mask11 = GetMask(x1, y1);

	const float upper = mask00 + (mask10 - mask00) * tx;
	const float lower = mask01 + (mask11 - mask01) * tx;

	return upper + (lower - upper) * ty;
}

bool Terrain::AddTerrainLayer(const std::string& baseColorPath, const std::string& normalPath)
{
	if (terrainLayers.size() >= MaxTerrainLayers)
	{
		terrainIoMessage = "Terrain layer add failed: max layer count reached.";
		return false;
	}

	if (baseColorPath.empty() || normalPath.empty())
	{
		terrainIoMessage = "Terrain layer add failed: path is empty.";
		return false;
	}

	const std::filesystem::path basePath = ResourceManager::Instance().ResolvePath(baseColorPath);
	const std::filesystem::path normalMapPath = ResourceManager::Instance().ResolvePath(normalPath);

	if (!std::filesystem::exists(basePath))
	{
		terrainIoMessage = "Terrain layer base color not found: " + basePath.generic_string();
		return false;
	}

	if (!std::filesystem::exists(normalMapPath))
	{
		terrainIoMessage = "Terrain layer normal not found: " + normalMapPath.generic_string();
		return false;
	}

	const std::string normalizedBasePath = basePath.generic_string();
	const std::string normalizedNormalPath = normalMapPath.generic_string();

	for (int i = 0; i < static_cast<int>(terrainLayers.size()); ++i)
	{
		if (terrainLayers[i].baseColorPath == normalizedBasePath)
		{
			currentTerrainLayerIndex = i;
			terrainIoMessage = "Terrain layer selected: " + terrainLayers[i].name;
			return true;
		}
	}

	TerrainLayer layer;
	layer.name = basePath.stem().string();
	layer.baseColorPath = normalizedBasePath;
	layer.normalPath = normalizedNormalPath;

	HRESULT hr = MipmapTexture::LoadTexture(
		Game::Graphics::Instance().GetDevice(),
		layer.baseColorPath.c_str(),
		layer.baseColorView.GetAddressOf());

	if (FAILED(hr))
	{
		terrainIoMessage = "Terrain layer base color load failed: " + layer.baseColorPath;
		return false;
	}

	hr = MipmapTexture::LoadTexture(
		Game::Graphics::Instance().GetDevice(),
		layer.normalPath.c_str(),
		layer.normalView.GetAddressOf());

	if (FAILED(hr))
	{
		terrainIoMessage = "Terrain layer normal load failed: " + layer.normalPath;
		return false;
	}

	terrainLayers.push_back(std::move(layer));
	currentTerrainLayerIndex = static_cast<int>(terrainLayers.size()) - 1;
	terrainIoMessage = "Terrain layer added: " + terrainLayers.back().name;
	return true;
}

float Terrain::GetTerrainLayerValue(int layerIndex) const
{
	if (terrainLayers.size() <= 1)
	{
		return 0.0f;
	}

	const int clampedIndex = std::clamp(
		layerIndex,
		0,
		static_cast<int>(terrainLayers.size()) - 1);

	return static_cast<float>(clampedIndex) /
		static_cast<float>(terrainLayers.size() - 1);
}

bool Terrain::SaveTerrainTexture(const std::string& filename)
{
	if (filename.empty())
	{
		terrainIoMessage = "Terrain save failed: filename is empty.";
		return false;
	}

	if (terrainPixels.size() != static_cast<size_t>(TerrainTextureWidth * TerrainTextureHeight))
	{
		terrainIoMessage = "Terrain save failed: terrain data size is invalid.";
		return false;
	}

	std::filesystem::path filepath(filename);
	if (ToLowerWString(filepath.extension().wstring()) != L".dds")
	{
		filepath.replace_extension(L".dds");
	}

	std::error_code error;
	if (!filepath.parent_path().empty())
	{
		std::filesystem::create_directories(filepath.parent_path(), error);
	}

	if (error)
	{
		terrainIoMessage = "Terrain save failed: directory creation failed.";
		return false;
	}

	DirectX::Image image{};
	image.width = TerrainTextureWidth;
	image.height = TerrainTextureHeight;
	image.format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	image.rowPitch = static_cast<size_t>(TerrainTextureWidth) * sizeof(Vector4);
	image.slicePitch = image.rowPitch * static_cast<size_t>(TerrainTextureHeight);
	image.pixels = reinterpret_cast<uint8_t*>(terrainPixels.data());

	const HRESULT hr = DirectX::SaveToDDSFile(
		image,
		DirectX::DDS_FLAGS_NONE,
		filepath.wstring().c_str());

	if (FAILED(hr))
	{
		terrainIoMessage = "Terrain save failed.";
		return false;
	}

	terrainFilePath = filepath.generic_string();
	terrainIoMessage = "Terrain saved: " + terrainFilePath;
	return true;
}

bool Terrain::LoadTerrainTexture(const std::string& filename)
{
	if (filename.empty())
	{
		terrainIoMessage = "Terrain load failed: filename is empty.";
		return false;
	}

	const std::filesystem::path filepath(filename);
	if (!std::filesystem::exists(filepath))
	{
		terrainIoMessage = "Terrain load failed: file not found.";
		return false;
	}

	DirectX::TexMetadata sourceMetadata{};
	DirectX::ScratchImage sourceImage;

	HRESULT hr = GpuResourceUtils::LoadImageFile(filepath, sourceMetadata, sourceImage);
	if (FAILED(hr))
	{
		terrainIoMessage = "Terrain load failed: image load error.";
		return false;
	}

	if (!LoadTerrainImage(sourceMetadata, sourceImage)) return false;
	terrainFilePath = filepath.generic_string();
	terrainIoMessage = "Terrain loaded. Collider rebuild is pending.";
	return true;
}

bool Terrain::SaveTerrainMemory(std::vector<uint8_t>& bytes) const
{
	if (terrainPixels.size() != static_cast<size_t>(TerrainTextureWidth * TerrainTextureHeight)) return false;
	DirectX::Image image{};
	image.width = TerrainTextureWidth;
	image.height = TerrainTextureHeight;
	image.format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	image.rowPitch = static_cast<size_t>(TerrainTextureWidth) * sizeof(Vector4);
	image.slicePitch = image.rowPitch * static_cast<size_t>(TerrainTextureHeight);
	image.pixels = reinterpret_cast<uint8_t*>(const_cast<Vector4*>(terrainPixels.data()));
	DirectX::Blob blob;
	if (FAILED(DirectX::SaveToDDSMemory(image, DirectX::DDS_FLAGS_NONE, blob))) return false;
	const auto* begin = static_cast<const uint8_t*>(blob.GetBufferPointer());
	bytes.assign(begin, begin + blob.GetBufferSize());
	return true;
}

bool Terrain::LoadTerrainMemory(const std::vector<uint8_t>& bytes)
{
	if (bytes.empty()) return false;
	DirectX::TexMetadata metadata{};
	DirectX::ScratchImage image;
	if (FAILED(DirectX::LoadFromDDSMemory(bytes.data(), bytes.size(), DirectX::DDS_FLAGS_NONE, &metadata, image))) return false;
	if (!LoadTerrainImage(metadata, image)) return false;
	terrainFilePath.clear();
	terrainIoMessage = "Terrain loaded from VSTG.";
	return true;
}

std::string Terrain::SaveSettingsJson() const
{
	const auto saveColor = [](const Color& value)
	{
		return json::array({value.x, value.y, value.z, value.w});
	};

	json root;
	root["navMeshMaskVersion"] = navMeshMaskInitialized ? 1 : 0;
	root["terrainSize"] = terrainSize;
	root["gridResolution"] = gridResolution;
	root["tessellation"] = {
		{"edge", tesselation_constant.edge_factor},
		{"inner", tesselation_constant.inner_factor},
		{"heightScaler", tesselation_constant.height_scaler},
		{"tilingScale", tesselation_constant.tilling_scale}};
	root["material"] = {
		{"baseColor", saveColor(baseColor)},
		{"emissiveColor", saveColor(emissiveColor)},
		{"metalness", metalness},
		{"roughness", roughness},
		{"occlusion", occlusion},
		{"occlusionStrength", occlusionStrength},
		{"shadowStrength", shadowStrength}};
	root["distanceFog"] = {
		{"enabled", distanceFogEnabled},
		{"start", distanceFogStart},
		{"end", distanceFogEnd},
		{"strength", distanceFogStrength},
		{"color", saveColor(distanceFogColor)}};
	root["grass"] = {
		{"density", grassSettings.density},
		{"width", grassSettings.width},
		{"height", grassSettings.height},
		{"sizeVariation", grassSettings.sizeVariation},
		{"windStrength", grassSettings.windStrength},
		{"windSpeed", grassSettings.windSpeed},
		{"drawDistance", grassSettings.drawDistance},
		{"tint", saveColor(grassSettings.tint)}};
	root["clearColor"] = saveColor(terrain_texture_clear_color);
	root["selectedLayer"] = currentTerrainLayerIndex;
	root["layers"] = json::array();
	for (const TerrainLayer& layer : terrainLayers)
	{
		root["layers"].push_back({
			{"baseColor", layer.baseColorPath},
			{"normal", layer.normalPath}});
	}
	root["brush"] = {
		{"mode", static_cast<int>(brushMode)},
		{"size", brush_size},
		{"heightStrength", heightBrushStrength},
		{"setHeight", setHeightValue},
		{"paintOpacity", paintOpacity},
		{"invertMask", invertBrushMask},
		{"selected", currentBrushIndex}};
	return root.dump();
}

bool Terrain::LoadSettingsJson(const std::string& text)
{
	if (text.empty()) return true;
	const auto loadColor = [](const json& value, const Color& fallback)
	{
		if (!value.is_array() || value.size() < 4) return fallback;
		return Color(
			value[0].get<float>(),
			value[1].get<float>(),
			value[2].get<float>(),
			value[3].get<float>());
	};

	try
	{
		const json root = json::parse(text);
		navMeshMaskInitialized = root.value("navMeshMaskVersion", 0) >= 1;
		terrainSize = root.value("terrainSize", terrainSize);
		gridResolution = root.value("gridResolution", gridResolution);
		if (const auto it = root.find("tessellation"); it != root.end())
		{
			tesselation_constant.edge_factor = it->value("edge", tesselation_constant.edge_factor);
			tesselation_constant.inner_factor = it->value("inner", tesselation_constant.inner_factor);
			tesselation_constant.height_scaler = it->value("heightScaler", tesselation_constant.height_scaler);
			tesselation_constant.tilling_scale = it->value("tilingScale", tesselation_constant.tilling_scale);
		}
		if (const auto it = root.find("material"); it != root.end())
		{
			if (it->contains("baseColor")) baseColor = loadColor((*it)["baseColor"], baseColor);
			if (it->contains("emissiveColor")) emissiveColor = loadColor((*it)["emissiveColor"], emissiveColor);
			metalness = it->value("metalness", metalness);
			roughness = it->value("roughness", roughness);
			occlusion = it->value("occlusion", occlusion);
			occlusionStrength = it->value("occlusionStrength", occlusionStrength);
			shadowStrength = it->value("shadowStrength", shadowStrength);
		}
		if (const auto it = root.find("distanceFog"); it != root.end())
		{
			distanceFogEnabled = it->value("enabled", distanceFogEnabled);
			distanceFogStart = it->value("start", distanceFogStart);
			distanceFogEnd = it->value("end", distanceFogEnd);
			distanceFogStrength = it->value("strength", distanceFogStrength);
			if (it->contains("color"))
				distanceFogColor = loadColor((*it)["color"], distanceFogColor);
		}
		if (const auto it = root.find("grass"); it != root.end())
		{
			grassSettings.density = it->value("density", grassSettings.density);
			grassSettings.width = it->value("width", grassSettings.width);
			grassSettings.height = it->value("height", grassSettings.height);
			grassSettings.sizeVariation = it->value("sizeVariation", grassSettings.sizeVariation);
			grassSettings.windStrength = it->value("windStrength", grassSettings.windStrength);
			grassSettings.windSpeed = it->value("windSpeed", grassSettings.windSpeed);
			grassSettings.drawDistance = it->value("drawDistance", grassSettings.drawDistance);
			if (it->contains("tint")) grassSettings.tint = loadColor((*it)["tint"], grassSettings.tint);
			grassDraftSettings = grassSettings;
			grassDraftInitialized = true;
			grassDirty = true;
		}
		if (root.contains("clearColor"))
			terrain_texture_clear_color = loadColor(root["clearColor"], terrain_texture_clear_color);

		terrainLayers.clear();
		for (const auto& layer : root.value("layers", json::array()))
			AddTerrainLayer(layer.value("baseColor", ""), layer.value("normal", ""));
		if (!terrainLayers.empty())
		{
			currentTerrainLayerIndex = std::clamp(
				root.value("selectedLayer", 0),
				0,
				static_cast<int>(terrainLayers.size()) - 1);
		}

		use_brush = false;
		if (const auto it = root.find("brush"); it != root.end())
		{
			brushMode = static_cast<BrushMode>(std::clamp(it->value("mode", 0), 0, 4));
			brush_size = std::clamp(it->value("size", brush_size), 1, 256);
			heightBrushStrength = it->value("heightStrength", heightBrushStrength);
			setHeightValue = it->value("setHeight", setHeightValue);
			paintOpacity = it->value("paintOpacity", paintOpacity);
			invertBrushMask = it->value("invertMask", invertBrushMask);
			currentBrushIndex = it->value("selected", currentBrushIndex);
		}
		MarkTerrainMeshDirty();
		return true;
	}
	catch (const json::exception&)
	{
		return false;
	}
}

bool Terrain::LoadTerrainImage(const DirectX::TexMetadata& sourceMetadata, const DirectX::ScratchImage& sourceImage)
{
	HRESULT hr = S_OK;
	const DirectX::ScratchImage* workingImage = &sourceImage;
	DirectX::ScratchImage convertedImage;

	if (sourceMetadata.format != DXGI_FORMAT_R32G32B32A32_FLOAT)
	{
		if (DirectX::IsCompressed(sourceMetadata.format))
		{
			hr = DirectX::Decompress(
				sourceImage.GetImages(),
				sourceImage.GetImageCount(),
				sourceMetadata,
				DXGI_FORMAT_R32G32B32A32_FLOAT,
				convertedImage);
		}
		else
		{
			hr = DirectX::Convert(
				sourceImage.GetImages(),
				sourceImage.GetImageCount(),
				sourceMetadata,
				DXGI_FORMAT_R32G32B32A32_FLOAT,
				DirectX::TEX_FILTER_DEFAULT,
				DirectX::TEX_THRESHOLD_DEFAULT,
				convertedImage);
		}

		if (FAILED(hr))
		{
			terrainIoMessage = "Terrain load failed: format conversion error.";
			return false;
		}

		workingImage = &convertedImage;
	}

	const DirectX::TexMetadata& metadata = workingImage->GetMetadata();
	if (metadata.width != TerrainTextureWidth || metadata.height != TerrainTextureHeight)
	{
		terrainIoMessage = "Terrain load failed: image must be 1024 x 1024.";
		return false;
	}

	const DirectX::Image* image = workingImage->GetImage(0, 0, 0);
	if (image == nullptr)
	{
		terrainIoMessage = "Terrain load failed: image data is empty.";
		return false;
	}

	navigationBaseHeights.clear();
	terrainPixels.resize(static_cast<size_t>(TerrainTextureWidth * TerrainTextureHeight));

	const size_t destinationRowPitch =
		static_cast<size_t>(TerrainTextureWidth) * sizeof(Vector4);

	for (int y = 0; y < TerrainTextureHeight; ++y)
	{
		const uint8_t* sourceRow =
			image->pixels + static_cast<size_t>(y) * image->rowPitch;

		uint8_t* destinationRow =
			reinterpret_cast<uint8_t*>(terrainPixels.data()) +
			static_cast<size_t>(y) * destinationRowPitch;

		memcpy(destinationRow, sourceRow, destinationRowPitch);
	}

	terrainTextureDirty = true;

	terrainEditHashDirty = true;
	is_terrain_texture_clear_color = false;
	MarkTerrainMeshDirty();

	return true;
}

std::filesystem::path Terrain::GetColliderVertexPath() const
{
	if (terrainFilePath.empty()) return {};
	std::filesystem::path filepath(terrainFilePath);
	filepath.replace_extension(".vx");
	return filepath;
}

bool Terrain::BuildGpuColliderMesh(
	float minX,
	float maxX,
	float minZ,
	float maxZ,
	std::vector<Vector3>& vertices,
	std::vector<uint32_t>& indices)
{
	std::vector<TerrainVertex> terrainVertices;
	BuildTerrainMesh(minX, maxX, minZ, maxZ, terrainVertices, indices);

	vertices.clear();
	vertices.reserve(terrainVertices.size());
	for (const TerrainVertex& vertex : terrainVertices)
	{
		vertices.push_back(vertex.position);
	}

	terrainIoMessage = "Terrain collider baked from render mesh.";
	return true;
}

void Terrain::BakeCollider()
{
	TerrainMeshCollider* collider = owner->GetComponent<TerrainMeshCollider>();
	if (!collider)
	{
		pendingColliderRebuild = true;
		return;
	}

	collider->RebuildFromTerrain();
	pendingColliderRebuild = false;
}

void Terrain::DrawTerrainLayerGUI()
{
	if (!ImGui::TreeNode((const char*)(ICON_FA_LAYER_GROUP " " u8"地形ペイントレイヤー")))
	{
		return;
	}

	ImGui::Text((const char*)u8"選択中: %s",
		terrainLayers.empty()
			? (const char*)u8"なし"
			: terrainLayers[currentTerrainLayerIndex].name.c_str());

	// ペイントレイヤーのサムネイル一覧
	{
		const float thumbnailSize = 72.0f;
		const float childHeight =
			thumbnailSize +
			ImGui::GetStyle().ScrollbarSize +
			ImGui::GetStyle().WindowPadding.y * 2.0f;

		ImGui::BeginChild(
			"terrain layer palette",
			ImVec2(0.0f, childHeight),
			true,
			ImGuiWindowFlags_HorizontalScrollbar);

		for (int i = 0; i < static_cast<int>(terrainLayers.size()); ++i)
		{
			ImGui::PushID(i);

			const bool selected = i == currentTerrainLayerIndex;
			if (selected)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.55f, 0.95f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.65f, 1.0f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.45f, 0.85f, 1.0f));
			}

			if (ImGui::ImageButton(
				"terrain layer",
				terrainLayers[i].baseColorView.Get(),
				ImVec2(thumbnailSize, thumbnailSize),
				ImVec2(0.0f, 0.0f),
				ImVec2(1.0f, 1.0f)))
			{
				currentTerrainLayerIndex = i;
			}

			if (selected)
			{
				ImGui::PopStyleColor(3);
			}

			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip(
					"%s\n%s\n%s",
					terrainLayers[i].name.c_str(),
					terrainLayers[i].baseColorPath.c_str(),
					terrainLayers[i].normalPath.c_str());
			}

			if (i + 1 < static_cast<int>(terrainLayers.size()))
			{
				ImGui::SameLine();
			}

			ImGui::PopID();
		}

		ImGui::EndChild();
	}

	ImGui::Text((const char*)u8"レイヤー数: %d / %d",
		static_cast<int>(terrainLayers.size()),
		MaxTerrainLayers);

	ImGui::TreePop();
}

void Terrain::DrawBrushGUI()
{
	if (!ImGui::TreeNode((const char*)(ICON_FA_IMAGE " " u8"ブラシテクスチャ")))
	{
		return;
	}

	const TerrainBrush* currentBrush = GetCurrentBrush();
	ImGui::Text((const char*)u8"選択中: %s",
		currentBrush != nullptr ? currentBrush->name.c_str() : (const char*)u8"なし");

	// ブラシテクスチャのサムネイル一覧
	{
		const float thumbnailSize = 72.0f;
		const float childHeight =
			thumbnailSize +
			ImGui::GetStyle().ScrollbarSize +
			ImGui::GetStyle().WindowPadding.y * 2.0f;

		ImGui::BeginChild(
			"brush texture palette",
			ImVec2(0.0f, childHeight),
			true,
			ImGuiWindowFlags_HorizontalScrollbar);

		for (int i = 0; i < static_cast<int>(brushes.size()); ++i)
		{
			ImGui::PushID(i);

			const bool selected = i == currentBrushIndex;
			if (selected)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.55f, 0.95f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.65f, 1.0f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.45f, 0.85f, 1.0f));
			}

			if (ImGui::ImageButton(
				"brush texture",
				brushes[i].shaderResourceView.Get(),
				ImVec2(thumbnailSize, thumbnailSize),
				ImVec2(0.0f, 0.0f),
				ImVec2(1.0f, 1.0f)))
			{
				SetBrushTexture(i);
			}

			if (selected)
			{
				ImGui::PopStyleColor(3);
			}

			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip(
					"%s\n%d x %d\n%s",
					brushes[i].name.c_str(),
					brushes[i].width,
					brushes[i].height,
					brushes[i].filepath.c_str());
			}

			if (i + 1 < static_cast<int>(brushes.size()))
			{
				ImGui::SameLine();
			}

			ImGui::PopID();
		}

		ImGui::EndChild();
	}

	ImGui::Checkbox((const char*)(ICON_FA_PAINT_BRUSH " " u8"ブラシマスクを反転"), &invertBrushMask);

	ImGui::Text((const char*)u8"ブラシ数: %d", static_cast<int>(brushes.size()));
	ImGui::TreePop();
}

void Terrain::DrawGUI()
{
	if (!grassDraftInitialized)
	{
		grassDraftSettings = grassSettings;
		grassDraftInitialized = true;
	}

	// よく使う地形ブラシ
	ImGui::TextUnformatted((const char*)(ICON_FA_PAINT_BRUSH " " u8"地形ブラシ"));
	ImGui::Checkbox((const char*)(ICON_FA_PAINT_BRUSH " " u8"ブラシを使用（B）"), &use_brush);

	int brushModeIndex = static_cast<int>(brushMode);
	const char* brushModeItems[] =
	{
		(const char*)u8"上げる／下げる",
		(const char*)u8"高さを指定",
		(const char*)u8"ペイント",
		(const char*)(ICON_FA_LEAF " " u8"草マップ"),
		(const char*)u8"NavMesh範囲",
	};

	if (ImGui::Combo(
			(const char*)(ICON_FA_PAINT_BRUSH " " u8"ブラシモード"), &brushModeIndex, brushModeItems,
			_countof(brushModeItems)))
	{
		brushMode = static_cast<BrushMode>(brushModeIndex);
		use_brush = true;
	}

	ImGui::DragInt((const char*)(ICON_FA_PAINT_BRUSH " " u8"ブラシサイズ"), &brush_size, 1.0f, 1, 256, "%d", ImGuiSliderFlags_AlwaysClamp);
	if (brushMode == BrushMode::RaiseLower)
	{
		ImGui::DragFloat(
			(const char*)(ICON_FA_COG " " u8"上下の強さ"), &heightBrushStrength, 0.001f, 0.0f, 1.0f);
	}
	else if (brushMode == BrushMode::SetHeight)
	{
		ImGui::DragFloat(
			(const char*)(ICON_FA_COG " " u8"指定する高さ"), &setHeightValue, 0.001f, -10.0f, 10.0f);
	}
	else if (brushMode == BrushMode::Paint)
	{
		ImGui::DragFloat(
			(const char*)(ICON_FA_PAINT_BRUSH " " u8"ペイント不透明度"), &paintOpacity, 0.001f, 0.0f, 1.0f);
	}
	else if (brushMode == BrushMode::GrassPaint)
	{
		ImGui::DragFloat(
			(const char*)(ICON_FA_COG " " u8"草の塗り強さ"), &paintOpacity, 0.001f, 0.0f, 1.0f);
	}
	else if (brushMode == BrushMode::NavMeshPaint)
	{
		ImGui::DragFloat((const char*)(ICON_FA_COG " " u8"塗り強さ"), &paintOpacity, 0.001f, 0.0f, 1.0f);
		ImGui::TextUnformatted((const char*)u8"左ドラッグ：範囲を追加 / Shift：範囲を削除");
		ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.3f, 1.0f), (const char*)u8"緑：入力範囲 / 赤：範囲外 / 水色の線：生成結果");
		if (NavMeshActor* navMesh = owner->GetComponent<NavMeshActor>())
		{
			if (ImGui::TreeNode((const char*)(ICON_FA_MAP " " u8"NavMesh生成設定")))
			{
				navMesh->DrawGUI();
				ImGui::TreePop();
			}
		}
	}
	ImGui::Separator();

	// ブラシとペイントレイヤー
	DrawBrushGUI();
	if (brushMode == BrushMode::Paint) DrawTerrainLayerGUI();

	// 地形のマテリアル
	if (ImGui::TreeNode((const char*)(ICON_FA_SLIDERS_H " " u8"地形PBR")))
	{
		ImGui::ColorEdit4((const char*)(ICON_FA_COG " " u8"基本色"), &baseColor.x);
		ImGui::ColorEdit4((const char*)(ICON_FA_COG " " u8"発光色"), &emissiveColor.x);
		ImGui::SliderFloat((const char*)(ICON_FA_COG " " u8"メタリック"), &metalness, 0.0f, 1.0f);
		ImGui::SliderFloat((const char*)(ICON_FA_COG " " u8"粗さ"), &roughness, 0.0001f, 1.0f);
		ImGui::SliderFloat((const char*)(ICON_FA_COG " " u8"オクルージョン"), &occlusion, 0.0f, 1.0f);
		ImGui::SliderFloat((const char*)(ICON_FA_COG " " u8"オクルージョン強度"), &occlusionStrength, 0.0f, 1.0f);
		ImGui::SliderFloat((const char*)(ICON_FA_COG " " u8"影の強度"), &shadowStrength, 0.0f, 1.0f);
		ImGui::TreePop();
	}

	if (ImGui::TreeNode((const char*)(ICON_FA_LEAF " " u8"草マップ")))
	{
		bool changed = false;
		changed |= ImGui::DragFloat((const char*)(ICON_FA_COG " " u8"密度"), &grassDraftSettings.density,
			0.05f, 0.0f, 0.0f);
		changed |= ImGui::DragFloat((const char*)(ICON_FA_COG " " u8"横幅"), &grassDraftSettings.width,
			0.01f, 0.05f, 5.0f);
		changed |= ImGui::DragFloat((const char*)(ICON_FA_COG " " u8"高さ"), &grassDraftSettings.height,
			0.01f, 0.05f, 8.0f);
		changed |= ImGui::SliderFloat((const char*)(ICON_FA_COG " " u8"大きさのばらつき"),
			&grassDraftSettings.sizeVariation, 0.0f, 0.9f);
		changed |= ImGui::DragFloat((const char*)(ICON_FA_COG " " u8"風の強さ"), &grassDraftSettings.windStrength,
			0.01f, 0.0f, 2.0f);
		changed |= ImGui::DragFloat((const char*)(ICON_FA_COG " " u8"風の速度"), &grassDraftSettings.windSpeed,
			0.01f, 0.0f, 8.0f);
		changed |= ImGui::DragFloat((const char*)(ICON_FA_COG " " u8"描画距離"), &grassDraftSettings.drawDistance,
			1.0f, 5.0f, 500.0f);
		changed |= ImGui::ColorEdit4((const char*)(ICON_FA_COG " " u8"草の色"), &grassDraftSettings.tint.x);

		if (changed)
		{
			grassSettings = grassDraftSettings;
			grassDirty = true;
		}
		int generatedTuftCount = 0;
		if (grassRenderer)
		{
			generatedTuftCount = grassRenderer->GetTuftCount();
		}
		ImGui::Text((const char*)u8"生成株数: %d", generatedTuftCount);
		ImGui::TextDisabled((const char*)u8"草マップはブラシモードから編集し、ブラシをOFFにすると確定します");
		ImGui::TreePop();
	}

	// 地形に適用する距離フォグ
	if (ImGui::TreeNode((const char*)(ICON_FA_CLOUD " " u8"距離フォグ")))
	{
		ImGui::Checkbox((const char*)(ICON_FA_COG " " u8"有効##TerrainDistanceFog"), &distanceFogEnabled);
		ImGui::DragFloat(
			(const char*)(ICON_FA_COG " " u8"開始距離##TerrainDistanceFog"),
			&distanceFogStart,
			0.5f,
			0.0f,
			std::max(distanceFogEnd - 0.1f, 0.0f));
		ImGui::DragFloat(
			(const char*)(ICON_FA_COG " " u8"終了距離##TerrainDistanceFog"),
			&distanceFogEnd,
			0.5f,
			distanceFogStart + 0.1f,
			1000.0f);
		ImGui::DragFloat(
			(const char*)(ICON_FA_COG " " u8"強度##TerrainDistanceFog"),
			&distanceFogStrength,
			0.01f,
			0.0f,
			1.0f);
		ImGui::ColorEdit3((const char*)(ICON_FA_COG " " u8"色##TerrainDistanceFog"), &distanceFogColor.x);
		ImGui::TreePop();
	}

	// 高さとペイントを保持する地形データ
	if (ImGui::TreeNode((const char*)(ICON_FA_DATABASE " " u8"地形データ")))
	{
		if (ImGui::Button((const char*)(ICON_FA_MOUNTAIN " " u8"地形テクスチャをクリア")))
		{
			ClearTerrainTexture();
		}

		ImGui::Text((const char*)u8"R：高さ / G：レイヤー / B：草 / A：NavMesh範囲");
		ImGui::DragFloat4((const char*)(ICON_FA_COG " " u8"クリア時のRGBA"), &terrain_texture_clear_color.x, 0.01f);

		if (terrainTextureShaderResourceView)
		{
			ImGui::Image(
				terrainTextureShaderResourceView.Get(),
				ImVec2(256, 256),
				ImVec2(0, 0),
				ImVec2(1, 1));
		}

		ImGui::TreePop();
	}

	// 地形メッシュとテセレーション
	if (ImGui::TreeNode((const char*)(ICON_FA_MOUNTAIN " " u8"地形レンダリング")))
	{
		float newTerrainSize = terrainSize;
		if (ImGui::DragFloat((const char*)(ICON_FA_MOUNTAIN " " u8"地形サイズ"), &newTerrainSize, 1.0f, 1.0f, 10000.0f))
		{
			terrainSize = newTerrainSize;
			MarkTerrainMeshDirty();
		}

		int newGridResolution = gridResolution;
		if (ImGui::DragInt((const char*)(ICON_FA_COG " " u8"グリッド解像度"), &newGridResolution, 1, 1, 256))
		{
			gridResolution = newGridResolution;
			MarkTerrainMeshDirty();
		}

		bool meshSettingChanged = false;
		meshSettingChanged |= ImGui::SliderFloat((const char*)(ICON_FA_COG " " u8"外周分割"), &tesselation_constant.edge_factor, 1.0f, 16.0f);
		meshSettingChanged |= ImGui::SliderFloat((const char*)(ICON_FA_COG " " u8"内部分割"), &tesselation_constant.inner_factor, 1.0f, 16.0f);
		if (ImGui::SliderFloat((const char*)(ICON_FA_COG " " u8"高さ倍率"), &tesselation_constant.height_scaler, -200.0f, 200.0f))
		{
			meshSettingChanged = true;
		}
		if (meshSettingChanged) MarkTerrainMeshDirty();
		ImGui::SliderFloat((const char*)(ICON_FA_COG " " u8"タイリング倍率"), &tesselation_constant.tilling_scale, 1.0f, 300.0f);

		ImGui::TreePop();
	}
}

void Terrain::ApplyDistanceFogSettings(RenderSettings& settings) const
{
	settings.distanceFogEnabled = distanceFogEnabled;
	settings.distanceFogStart = distanceFogStart;
	settings.distanceFogEnd = distanceFogEnd;
	settings.distanceFogStrength = distanceFogStrength;
	settings.distanceFogColor = distanceFogColor;
}

float Terrain::GetHeightByUV(float u, float v) const
{
	if (terrainPixels.empty())
	{
		return 0.0f;
	}

	u = std::clamp(u, 0.0f, 1.0f);
	v = std::clamp(v, 0.0f, 1.0f);

	int x = std::clamp(
		static_cast<int>(std::floor(u * static_cast<float>(TerrainTextureWidth))),
		0,
		TerrainTextureWidth - 1);

	int y = std::clamp(
		static_cast<int>(std::floor(v * static_cast<float>(TerrainTextureHeight))),
		0,
		TerrainTextureHeight - 1);

	const Vector4& pixel = terrainPixels[
		static_cast<size_t>(y) * TerrainTextureWidth +
		static_cast<size_t>(x)];

	return pixel.x * tesselation_constant.height_scaler;
}

float Terrain::GetNavigationHeightByUV(float u, float v) const
{
	if (navigationBaseHeights.empty()) return GetHeightByUV(u, v);
	const int x = std::clamp(static_cast<int>(floorf(u * TerrainTextureWidth)), 0, TerrainTextureWidth - 1);
	const int y = std::clamp(static_cast<int>(floorf(v * TerrainTextureHeight)), 0, TerrainTextureHeight - 1);
	return navigationBaseHeights[static_cast<size_t>(y) * TerrainTextureWidth + x] * tesselation_constant.height_scaler;
}

float Terrain::GetNavMeshMaskByUV(float u, float v) const
{
	if (terrainPixels.empty() || u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f) return 0.0f;
	if (!navMeshMaskInitialized) return 1.0f;
	const int x = std::min(static_cast<int>(u * TerrainTextureWidth), TerrainTextureWidth - 1);
	const int y = std::min(static_cast<int>(v * TerrainTextureHeight), TerrainTextureHeight - 1);
	return std::clamp(terrainPixels[static_cast<size_t>(y) * TerrainTextureWidth + x].w, 0.0f, 1.0f);
}

void Terrain::SetNavMeshMaskByUV(float u, float v, float value)
{
	if (terrainPixels.empty()) return;
	const int x = std::clamp(static_cast<int>(u * TerrainTextureWidth), 0, TerrainTextureWidth - 1);
	const int y = std::clamp(static_cast<int>(v * TerrainTextureHeight), 0, TerrainTextureHeight - 1);
	terrainPixels[static_cast<size_t>(y) * TerrainTextureWidth + x].w = std::clamp(value, 0.0f, 1.0f);
	navMeshMaskInitialized = true;
	terrainTextureDirty = true;
	terrainEditHashDirty = true;
}

float Terrain::GetSurfaceHeightByUV(float u, float v, bool navigationBase) const
{
	u = std::clamp(u, 0.0f, 1.0f);
	v = std::clamp(v, 0.0f, 1.0f);

	const int tessellationFactor = std::max(
		static_cast<int>(std::ceil(std::max(
			tesselation_constant.edge_factor,
			tesselation_constant.inner_factor))),
		1);
	const int meshSegments = std::max(gridResolution * tessellationFactor, 1);
	const float gridX = u * meshSegments;
	const float gridZ = v * meshSegments;
	const int x = std::clamp(static_cast<int>(std::floor(gridX)), 0, meshSegments - 1);
	const int z = std::clamp(static_cast<int>(std::floor(gridZ)), 0, meshSegments - 1);
	const float xRatio = gridX - x;
	const float zRatio = gridZ - z;
	const float u0 = static_cast<float>(x) / meshSegments;
	const float u1 = static_cast<float>(x + 1) / meshSegments;
	const float v0 = static_cast<float>(z) / meshSegments;
	const float v1 = static_cast<float>(z + 1) / meshSegments;
	const float height00 = (navigationBase ? GetNavigationHeightByUV(u0, v0) : GetHeightByUV(u0, v0));
	const float height10 = (navigationBase ? GetNavigationHeightByUV(u1, v0) : GetHeightByUV(u1, v0));
	const float height01 = (navigationBase ? GetNavigationHeightByUV(u0, v1) : GetHeightByUV(u0, v1));

	if (xRatio + zRatio <= 1.0f)
	{
		return height00 +
			(height10 - height00) * xRatio +
			(height01 - height00) * zRatio;
	}

	const float height11 = (navigationBase ? GetNavigationHeightByUV(u1, v1) : GetHeightByUV(u1, v1));
	return height10 * (1.0f - zRatio) +
		height01 * (1.0f - xRatio) +
		height11 * (xRatio + zRatio - 1.0f);
}

Vector3 Terrain::GetSurfaceNormalByUV(float u, float v, bool navigationBase) const
{
	u = std::clamp(u, 0.0f, 1.0f);
	v = std::clamp(v, 0.0f, 1.0f);

	const int tessellationFactor = std::max(
		static_cast<int>(std::ceil(std::max(
			tesselation_constant.edge_factor,
			tesselation_constant.inner_factor))),
		1);
	const int meshSegments = std::max(gridResolution * tessellationFactor, 1);
	const float gridX = u * meshSegments;
	const float gridZ = v * meshSegments;
	const int x = std::clamp(static_cast<int>(std::floor(gridX)), 0, meshSegments - 1);
	const int z = std::clamp(static_cast<int>(std::floor(gridZ)), 0, meshSegments - 1);
	const float xRatio = gridX - x;
	const float zRatio = gridZ - z;
	const float u0 = static_cast<float>(x) / meshSegments;
	const float u1 = static_cast<float>(x + 1) / meshSegments;
	const float v0 = static_cast<float>(z) / meshSegments;
	const float v1 = static_cast<float>(z + 1) / meshSegments;

	const Vector3 position00 = {
		(u0 - 0.5f) * terrainSize,
		(navigationBase ? GetNavigationHeightByUV(u0, v0) : GetHeightByUV(u0, v0)),
		(v0 - 0.5f) * terrainSize};
	const Vector3 position10 = {
		(u1 - 0.5f) * terrainSize,
		(navigationBase ? GetNavigationHeightByUV(u1, v0) : GetHeightByUV(u1, v0)),
		(v0 - 0.5f) * terrainSize};
	const Vector3 position01 = {
		(u0 - 0.5f) * terrainSize,
		(navigationBase ? GetNavigationHeightByUV(u0, v1) : GetHeightByUV(u0, v1)),
		(v1 - 0.5f) * terrainSize};

	Vector3 normal;
	if (xRatio + zRatio <= 1.0f)
	{
		// 草を配置した地形三角形と同じ面から傾きを求める
		normal = (position01 - position00).Cross(position10 - position00);
	}
	else
	{
		const Vector3 position11 = {
			(u1 - 0.5f) * terrainSize,
			(navigationBase ? GetNavigationHeightByUV(u1, v1) : GetHeightByUV(u1, v1)),
			(v1 - 0.5f) * terrainSize};
		normal = (position01 - position10).Cross(position11 - position10);
	}

	if (normal.LengthSquared() <= eps)
	{
		return Vector3::UnitY;
	}

	normal.Normalize();
	return normal;
}

float Terrain::GetGrassMaskByUV(float u, float v) const
{
	if (terrainPixels.empty()) return 0.0f;

	const float clampedU = std::clamp(u, 0.0f, 1.0f);
	const float clampedV = std::clamp(v, 0.0f, 1.0f);
	const int x = std::clamp(
		static_cast<int>(clampedU * static_cast<float>(TerrainTextureWidth)),
		0, TerrainTextureWidth - 1);
	const int y = std::clamp(
		static_cast<int>(clampedV * static_cast<float>(TerrainTextureHeight)),
		0, TerrainTextureHeight - 1);
	const Vector4& pixel = terrainPixels[
		static_cast<size_t>(y) * TerrainTextureWidth + static_cast<size_t>(x)];
	return std::clamp(pixel.z, 0.0f, 1.0f);
}

// ワールド座標に描かれている割合が最も大きい地形レイヤー番号を返す
int Terrain::GetSurfaceLayerIndex(const Vector3& worldPosition) const
{
	if (terrainPixels.empty() || terrainLayers.empty()) return -1;
	const Transform* transform = owner ? owner->GetComponent<Transform>() : nullptr;
	if (!transform) return -1;

	const Vector3 localPosition = Vector3::Transform(worldPosition, transform->matrix.Invert());
	const float u = localPosition.x / terrainSize + 0.5f;
	const float v = localPosition.z / terrainSize + 0.5f;
	return GetSurfaceLayerIndexByUV(u, v);
}

int Terrain::GetSurfaceLayerIndexByUV(float u, float v) const
{
	if (terrainPixels.empty() || terrainLayers.empty() ||
		u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f)
		return -1;

	const int x = std::clamp(
		static_cast<int>(std::floor(u * static_cast<float>(TerrainTextureWidth))),
		0, TerrainTextureWidth - 1);
	const int y = std::clamp(
		static_cast<int>(std::floor(v * static_cast<float>(TerrainTextureHeight))),
		0, TerrainTextureHeight - 1);
	const float layerRate = std::clamp(terrainPixels[
		static_cast<size_t>(y) * TerrainTextureWidth + static_cast<size_t>(x)].y, 0.0f, 1.0f);
	return std::clamp(
		static_cast<int>(std::round(layerRate * static_cast<float>(terrainLayers.size() - 1))),
		0, static_cast<int>(terrainLayers.size()) - 1);
}

// ワールド座標に描かれている主地形レイヤー名を返す
std::string Terrain::GetSurfaceLayerName(const Vector3& worldPosition) const
{
	const int index = GetSurfaceLayerIndex(worldPosition);
	return index >= 0 && index < static_cast<int>(terrainLayers.size())
		? terrainLayers[index].name : std::string{};
}

void Terrain::PlayGrassRemovalEffect(const Vector3& worldPosition)
{
	if (grassRemovalEffectCooldown > 0.0f) return;
	auto& effects = EffectManager::Instance();
	if (!grassRemovalEffect) grassRemovalEffect = effects.LoadEffect(EffectId::FEATHER);
	effects.PlayDetached(grassRemovalEffect, 
		 Matrix::CreateScale(0.3f) * Matrix::CreateTranslation(worldPosition));
	grassRemovalEffectCooldown = 0.08f;
}

// sphereの中にある草マップを範囲内で削除する
bool Terrain::RemoveGrassInSphere(const Vector3& worldCenter, float worldRadius, LayerId sourceLayer)
{
	if (!PhysicsLayerManager::Instance().Collides(sourceLayer, Layers::Get("GrassMap"))) return false;
	if (terrainPixels.empty() || worldRadius <= 0.0f || !std::isfinite(worldRadius) ||
		!std::isfinite(worldCenter.x) || !std::isfinite(worldCenter.y) || !std::isfinite(worldCenter.z)) return false;
	const Transform* transform = owner ? owner->GetTransform() : nullptr;
	if (!transform) return false;
	const Matrix& world = transform->matrix;
	const Matrix inverse = world.Invert();
	const Vector3 center = Vector3::Transform(worldCenter, inverse);
	const float radiusX = worldRadius * Vector3(inverse._11, inverse._21, inverse._31).Length();
	const float radiusZ = worldRadius * Vector3(inverse._13, inverse._23, inverse._33).Length();
	const float halfSize = terrainSize * 0.5f;
	if (center.x + radiusX < -halfSize || center.x - radiusX > halfSize ||
		center.z + radiusZ < -halfSize || center.z - radiusZ > halfSize) return false;
	const auto texel = [&](float coordinate, int count) {
		return static_cast<int>(std::clamp(coordinate / terrainSize + 0.5f, 0.0f, 1.0f) * (count - 1));
	};
	const int x0 = std::max(texel(center.x - radiusX, TerrainTextureWidth) - 1, 0);
	const int x1 = std::min(texel(center.x + radiusX, TerrainTextureWidth) + 1, TerrainTextureWidth - 1);
	const int z0 = std::max(texel(center.z - radiusZ, TerrainTextureHeight) - 1, 0);
	const int z1 = std::min(texel(center.z + radiusZ, TerrainTextureHeight) + 1, TerrainTextureHeight - 1);
	Vector3 removedPosition = Vector3::Zero;
	int removedCount = 0;
	const float grassHeight = grassSettings.height * (1.0f + grassSettings.sizeVariation);
	const Vector3 blade = Vector3::TransformNormal(Vector3(0.0f, grassHeight, 0.0f), world);
	const float lengthSquared = blade.LengthSquared();
	const float radiusSquared = worldRadius * worldRadius;
	for (int z = z0; z <= z1; ++z)
	{
		for (int x = x0; x <= x1; ++x)
		{
			Vector4& pixel = terrainPixels[static_cast<size_t>(z) * TerrainTextureWidth + x];
			if (pixel.z < 0.5f) continue;
			const float u = (static_cast<float>(x) + 0.5f) / TerrainTextureWidth;
			const float v = (static_cast<float>(z) + 0.5f) / TerrainTextureHeight;
			const Vector3 localRoot((u - 0.5f) * terrainSize, GetSurfaceHeightByUV(u, v), (v - 0.5f) * terrainSize);
			const Vector3 root = Vector3::Transform(localRoot, world);
			const float t = lengthSquared > eps
				? std::clamp((worldCenter - root).Dot(blade) / lengthSquared, 0.0f, 1.0f) : 0.0f;
			if ((worldCenter - root - blade * t).LengthSquared() > radiusSquared) continue;
			pixel.z = 0.0f;
			removedPosition += root;
			++removedCount;
		}
	}
	if (!removedCount) return false;
	terrainTextureDirty = true;
	terrainEditHashDirty = true;
	is_terrain_texture_clear_color = false;
	// 既存の草頂点はGPU側のマスクで消すため再生成しない。
	PlayGrassRemovalEffect(removedPosition / static_cast<float>(removedCount));
	return true;
}

void Terrain::Deform(
	const Vector3& worldPosition, const Vector3& direction, float power, float requestedRadius)
{
	if (terrainPixels.empty() || power <= eps || direction.LengthSquared() <= eps ||
		fabsf(tesselation_constant.height_scaler) <= eps)
	{
		return;
	}

	Transform* transform = owner->GetComponent<Transform>();
	if (!transform) return;

	const Matrix inverseWorld = transform->matrix.Invert();
	const Vector3 localPosition = Vector3::Transform(worldPosition, inverseWorld);
	Vector3 localDirection = Vector3::TransformNormal(direction, inverseWorld);
	if (localDirection.LengthSquared() <= eps) return;
	localDirection.Normalize();

	const float heightOffset = localDirection.y * power;
	if (fabsf(heightOffset) <= eps) return;

	// 描画メッシュの頂点間隔より小さい変形は高さマップに書かれても見た目へ現れない。
	// 少なくとも中心の周囲に複数頂点が入る半径を確保して、着地跡を描画へ反映する。
	const int meshSegments = std::max(
		gridResolution * std::max(
			static_cast<int>(std::ceil(std::max(
				tesselation_constant.edge_factor,
				tesselation_constant.inner_factor))),
			1),
		1);
	const float meshSpacing = terrainSize / static_cast<float>(meshSegments);
	const float radius = std::max({
		requestedRadius,
		power,
		terrainSize / static_cast<float>(TerrainTextureWidth),
		meshSpacing * 2.0f});
	// へこみの縁に草が残らないように少し広く消す
	const float grassRemovalMargin = std::max(grassSettings.height, 0.5f);
	const float grassRemovalRadius = radius + grassRemovalMargin;
	const float u = localPosition.x / terrainSize + 0.5f;
	const float v = localPosition.z / terrainSize + 0.5f;
	const int centerX = static_cast<int>(u * static_cast<float>(TerrainTextureWidth - 1));
	const int centerY = static_cast<int>(v * static_cast<float>(TerrainTextureHeight - 1));
	const int pixelRadius = std::max(static_cast<int>(ceilf(
		grassRemovalRadius / terrainSize * static_cast<float>(TerrainTextureWidth - 1))), 1);

	const int x0 = std::max(centerX - pixelRadius, 0);
	const int y0 = std::max(centerY - pixelRadius, 0);
	const int x1 = std::min(centerX + pixelRadius, TerrainTextureWidth - 1);
	const int y1 = std::min(centerY + pixelRadius, TerrainTextureHeight - 1);
	if (x0 > x1 || y0 > y1) return;

	if (navigationBaseHeights.empty())
	{
		navigationBaseHeights.reserve(terrainPixels.size());
		for (const Vector4& pixel : terrainPixels) navigationBaseHeights.push_back(pixel.x);
	}
	const float texelSize = terrainSize / static_cast<float>(TerrainTextureWidth - 1);
	Vector3 removedGrassPosition = Vector3::Zero;
	int removedGrassCount = 0;
	for (int y = y0; y <= y1; ++y)
	{
		for (int x = x0; x <= x1; ++x)
		{
			const float deltaX = static_cast<float>(x - centerX) * texelSize;
			const float deltaZ = static_cast<float>(y - centerY) * texelSize;
			const float distance = sqrtf(deltaX * deltaX + deltaZ * deltaZ);
			if (distance > grassRemovalRadius) continue;

			Vector4& pixel = terrainPixels[
				static_cast<size_t>(y) * TerrainTextureWidth + static_cast<size_t>(x)];
			if (pixel.z >= 0.5f)
			{
				removedGrassPosition += Vector3(
					(static_cast<float>(x) / (TerrainTextureWidth - 1) - 0.5f) * terrainSize,
					pixel.x * tesselation_constant.height_scaler,
					(static_cast<float>(y) / (TerrainTextureHeight - 1) - 0.5f) * terrainSize);
				++removedGrassCount;
			}
			pixel.z = 0.0f;
			if (distance <= radius)
			{
				float falloff = 1.0f - distance / radius;
				falloff = falloff * falloff * (3.0f - 2.0f * falloff);
				pixel.x += heightOffset * falloff / tesselation_constant.height_scaler;
			}
		}
	}

	terrainTextureDirty = true;

	terrainEditHashDirty = true;
	is_terrain_texture_clear_color = false;
	// 既存の草頂点はGPU側のマスクで消すため再生成しない
	MarkTerrainMeshDirty();
	if (removedGrassCount)
		PlayGrassRemovalEffect(Vector3::Transform(
			removedGrassPosition / static_cast<float>(removedGrassCount), transform->matrix));

	if (TerrainMeshCollider* collider = owner->GetComponent<TerrainMeshCollider>())
	{
		const float radiusUv = radius / terrainSize;
		collider->RebuildRegionFromTerrain(
			u - radiusUv, u + radiusUv, v - radiusUv, v + radiusUv);
		pendingColliderRebuild = false;
	}

	// 戦闘中の変形では通行範囲を維持し、接地だけコライダーで更新する。
}

uint64_t Terrain::GetTerrainDataHash(bool includeMasks) const
{
	if (includeMasks && !terrainEditHashDirty) return terrainEditHash;
	uint64_t hash = 14695981039346656037ull;

	for (const Vector4& pixel : terrainPixels)
	{
		const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&pixel.x);
		for (size_t i = 0; i < (includeMasks ? sizeof(pixel) : sizeof(pixel.x)); ++i)
		{
			hash ^= bytes[i];
			hash *= 1099511628211ull;
		}
	}

	if (includeMasks)
	{
		terrainEditHash = hash;
		terrainEditHashDirty = false;
	}
	return hash;
}
