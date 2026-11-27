#include <Dx11Kit/Dx11Kit.h>
#include <ProfileKit/ProfileKit.h>

#include "IconsFontAwesome5.h"
#include "Rendering/Component/Terrain.h"

#include "Gameplay/Actor/Actor.h"
#include "Rendering/Core/Graphics.h"
#include "Resource/Texture.h"
#include "Resource/ImageData.h"
#include "Gameplay/Lighting/LightManager.h"
#include "Application/Time/GameTime.h"
#include "Application/SettingsAndDebug/DebugUtil.h"
#include "Physics/Collider/TerrainMeshCollider.h"
#include "Physics/Navigation/NavMeshActor.h"
#include "Resource/Texture.h"
#include "Resource/ResourceManager.h"
#include "Resource/ResourcePath.h"
#include "Core/Foundation/Json.h"
#include "Rendering/Effect/Effect.h"
#include "Rendering/Effect/EffectManager.h"
#include <DirectXTex.h>

#include <cfloat>
#include <cmath>
#include <stdexcept>

Terrain::Terrain(Object* owner)
	: Component(owner)
{
}

void Terrain::OnAwake()
{
	InitializeGpuResources();
	grassRenderer = std::make_unique<TerrainGrassRenderer>(Game::Graphics::Instance().GetDevice());
	ClearTerrainTexture();
}

void Terrain::AddDecal(const std::string& path, const Transform& transform, float opacity)
{
	if (terrainDecals.size() >= MaxTerrainDecals)
		terrainDecals.erase(terrainDecals.begin());
	if (path.empty() || std::abs(transform.scale.x) < 0.0001f ||
		std::abs(transform.scale.z) < 0.0001f)
		throw std::invalid_argument("Invalid terrain decal parameters.");

	auto texture = ResourceManager::Instance().LoadTexture(path, ResourceManager::ResourceType::DecalTex);
	if (!texture) throw std::runtime_error("Terrain decal texture could not be loaded: " + path);
	terrainDecals.push_back({transform, path, std::move(texture), std::clamp(opacity, 0.0f, 1.0f)});
}

void Terrain::BindDecals(ID3D11DeviceContext* dc)
{
	CbTerrainDecal constants{};
	ID3D11ShaderResourceView* textures[MaxTerrainDecals] = {};
	for (const auto& decal : terrainDecals)
	{
		if (!decal.texture || decal.opacity <= 0.0f) continue;

		// 真上からの投影。高さやX/Z回転に依存させず、Y回転だけを使用する。
		const Vector3 forward = decal.transform.GetForward();
		const float yaw = std::atan2(forward.x, forward.z);
		const Matrix world = Matrix::CreateScale(decal.transform.scale.x, 1.0f, decal.transform.scale.z)
			* Matrix::CreateRotationY(yaw)
			* Matrix::CreateTranslation(decal.transform.position.x, 0.0f, decal.transform.position.z);
		const int index = constants.count++;
		constants.worldToDecal[index] = world.Invert();
		constants.parameters[index] = Vector4(decal.opacity, 0.0f, 0.0f, 0.0f);
		textures[index] = decal.texture->GetShaderResourceView().Get();
	}
	terrainDecalConstantBuffer->Update(dc, &constants);
	terrainDecalConstantBuffer->BindPS(dc, 5);
	Gpu::PixelShader::SetShaderResources(dc, 52, textures);
}

void Terrain::ClearDecals()
{
	terrainDecals.clear();
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

	shadowMapConstantBuffer = std::make_unique<Gpu::ConstantBuffer>(device, sizeof(CbShadowMap));

	materialConstantBuffer = std::make_unique<Gpu::ConstantBuffer>(device, sizeof(CbMaterial));

	terrainObjectConstantBuffer = std::make_unique<Gpu::ConstantBuffer>(device, sizeof(CbTerrainObject));

	terrainSceneConstantBuffer = std::make_unique<Gpu::ConstantBuffer>(device, sizeof(CbTerrainScene));

	tesselationConstantBuffer = std::make_unique<Gpu::ConstantBuffer>(device, sizeof(CbTessellation));

	terrainLayerConstantBuffer = std::make_unique<Gpu::ConstantBuffer>(device, sizeof(CbTerrainLayer));
	static_assert(sizeof(CbTerrainDecal) == MaxTerrainDecals * 80 + 16);
	terrainDecalConstantBuffer = std::make_unique<Gpu::ConstantBuffer>(device, sizeof(CbTerrainDecal));

	terrainColliderBuildConstantBuffer = std::make_unique<Gpu::ConstantBuffer>(device, sizeof(CbTerrainColliderBuild));

	CreateGridMesh(device);
	CreateTerrainTexture(device);

	const D3D11_INPUT_ELEMENT_DESC inputElementDescs[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};

	terrainVertexShader = std::make_unique<Gpu::VertexShader>(device, ResourceManager::Instance().ResolveShaderPath("Resources/Shader/TerrainPrimitiveMeshVS.cso").c_str(),
		inputElementDescs);

	terrainHullShader = std::make_unique<Gpu::HullShader>(device, ResourceManager::Instance().ResolveShaderPath("Resources/Shader/TerrainPrimitiveHS.cso").c_str());

	terrainDomainShader = std::make_unique<Gpu::DomainShader>(device, ResourceManager::Instance().ResolveShaderPath("Resources/Shader/TerrainPrimitiveDS.cso").c_str());

	terrainPixelShader = std::make_unique<Gpu::PixelShader>(device, ResourceManager::Instance().ResolveShaderPath("Resources/Shader/TerrainPrimitivePS.cso").c_str());

	terrainColliderBuildComputeShader = std::make_unique<Gpu::ComputeShader>(device, ResourceManager::Instance().ResolveShaderPath("Resources/Shader/TerrainColliderBuildCS.cso").c_str());

	defaultBaseTexture = std::make_shared<Texture>(device, 0xFFFFFFFFu);
	defaultNormalTexture = std::make_shared<Texture>(device, 0xFFFF7F7Fu);
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

	vertexBuffer = std::make_unique<Gpu::VertexBuffer>(device, vertexBufferDesc, &vertexData);

	D3D11_BUFFER_DESC indexBufferDesc{};
	indexBufferDesc.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * indices.size());
	indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA indexData{};
	indexData.pSysMem = indices.data();

	indexBuffer = std::make_unique<Gpu::IndexBuffer>(device, indexBufferDesc, &indexData);

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
	PROFILE_FUNCTION();
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

	// 指定されたUV範囲を、描画と同じ分割数のグリッドへ揃える。
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

	// 各三角形の法線を頂点へ加算し、隣接面の向きを平均する。
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
	if (!terrainTextureDirty) return;

	Texture::Update(dc, terrainTexture.Get(), terrainPixels.data(), static_cast<UINT>(sizeof(Vector4) * TerrainTextureWidth));

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
	cbObject.world = transform->GetMatrix();
	cbObject.terrainSize = terrainSize;
	cbObject.heightMapTexelSize = 1.0f / static_cast<float>(TerrainTextureWidth);

	terrainObjectConstantBuffer->Update(dc, &cbObject);
}

void Terrain::UpdateTerrainSceneConstantBuffer(
	ID3D11DeviceContext* dc,
	const RenderContext& rc)
{
	CbTerrainScene cbScene{};
	cbScene.viewProjection = rc.camera->GetView() * rc.camera->GetProjection();
	cbScene.viewPosition = rc.camera->GetEye();
	cbScene.shadingMode = static_cast<float>(rc.renderSettings.shadingMode);
	cbScene.lightData = rc.lightManager->ConvertToCb();
	cbScene.distanceFogColor = rc.renderSettings.distanceFogColor;
	cbScene.distanceFogParams = {
		rc.renderSettings.distanceFogStart,
		rc.renderSettings.distanceFogEnd,
		rc.renderSettings.distanceFogStrength,
		rc.renderSettings.distanceFogEnabled ? 1.0f : 0.0f};
	terrainSceneConstantBuffer->Update(dc, &cbScene);
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

	shadowMapConstantBuffer->Update(dc, &cbShadow);
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

	materialConstantBuffer->Update(dc, &cbMaterial);
}

void Terrain::PrepareRenderResources()
{
	if (terrainMeshDirty) CreateGridMesh(Game::Graphics::Instance().GetDevice());
	if (grassRenderer && grassDirty)
	{
		grassRenderer->Rebuild(*this, grassSettings);
		grassDirty = false;
	}
}

void Terrain::Update()
{
	PROFILE_FUNCTION();
	grassRemovalEffectCooldown.Update(Game::Time::deltaTime);
}

void Terrain::Render(const RenderContext& rc)
{
	PROFILE_FUNCTION();
	ID3D11DeviceContext* dc = rc.deviceContext;

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

	tesselationConstantBuffer->Update(dc, &tesselation_constant);

	CbTerrainLayer cbTerrainLayer{};
	cbTerrainLayer.layerCount = std::max(static_cast<int>(terrainLayers.size()), 1);
	const auto* preview = rc.renderSettings.terrainBrushPreview;
	if (preview && preview->terrain != this) preview = nullptr;
	if (preview)
	{
		cbTerrainLayer.brushPreviewMode = preview->mode;
		cbTerrainLayer.brushFlags = preview->flags;
		cbTerrainLayer.brushArea = preview->area;
		cbTerrainLayer.brushParams = preview->parameters;
	}
	terrainLayerConstantBuffer->Update(dc, &cbTerrainLayer);

	rc.graphics->OpaqueBlendState->Bind(dc);

	rc.graphics->DepthWriteState->Bind(dc);

	(rc.renderSettings.shadingMode == ShadingMode::Wire
		? *rc.graphics->WireframeRasterizerState : *rc.graphics->SolidRasterizerState).Bind(dc);

	UINT stride = sizeof(TerrainVertex);
	UINT offset = 0;

	vertexBuffer->Bind(dc, stride, 0, offset);
	indexBuffer->Bind(dc, DXGI_FORMAT_R32_UINT, 0);
	Gpu::VertexBuffer::SetTopology(dc, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	terrainVertexShader->Bind(dc);
	Gpu::HullShader::Unbind(dc);
	Gpu::DomainShader::Unbind(dc);
	terrainPixelShader->Bind(dc);

	shadowMapConstantBuffer->BindVS(dc, 0);
	terrainObjectConstantBuffer->BindVS(dc, 3);
	terrainSceneConstantBuffer->BindVS(dc, 7);
	Gpu::ConstantBuffer::BindPS(dc, 0, {shadowMapConstantBuffer.get(), materialConstantBuffer.get(), tesselationConstantBuffer.get()});
	terrainLayerConstantBuffer->BindPS(dc, 4);
	terrainSceneConstantBuffer->BindPS(dc, 7);

	ID3D11ShaderResourceView* terrainSrv = terrainTextureShaderResourceView.Get();
	Gpu::PixelShader::SetShaderResources(dc, 0, terrainSrv);
	ID3D11ShaderResourceView* brushSrv = preview ? preview->texture : nullptr;
	Gpu::PixelShader::SetShaderResources(dc, 1, brushSrv);

	Gpu::PixelShader::SetShaderResources(dc, 8, rc.shadowMapData.shadowMaps);

	ID3D11ShaderResourceView* iblSrvs[] =
	{
		rc.iblData.ggxLookUpTableMap,
		rc.iblData.specularPremappingRadianceEnvironmentMap,
		rc.iblData.diffuseIrradianceEnvironmentMap,
	};
	Gpu::PixelShader::SetShaderResources(dc, 17, iblSrvs);

	ID3D11ShaderResourceView* terrainBaseColorSrvs[MaxTerrainLayers] = {};
	ID3D11ShaderResourceView* terrainNormalSrvs[MaxTerrainLayers] = {};
	for (int i = 0; i < static_cast<int>(terrainLayers.size()) && i < MaxTerrainLayers; ++i)
	{
		terrainBaseColorSrvs[i] = terrainLayers[i].baseColorView.Get();
		terrainNormalSrvs[i] = terrainLayers[i].normalView.Get();
	}

	if (terrainLayers.empty())
	{
		terrainBaseColorSrvs[0] = defaultBaseTexture->GetShaderResourceView().Get();
		terrainNormalSrvs[0] = defaultNormalTexture->GetShaderResourceView().Get();
	}
	Gpu::PixelShader::SetShaderResources(dc, 20, terrainBaseColorSrvs);
	Gpu::PixelShader::SetShaderResources(dc, 36, terrainNormalSrvs);
	BindDecals(dc);

	Gpu::SamplerState::BindPS(dc, 0, {rc.graphics->PointClampSamplerState.get(), rc.graphics->LinearClampSamplerState.get(), rc.graphics->AnisotropicWrapSamplerState.get()});

	indexBuffer->Draw(dc, indexCount);
	if (rc.renderSettings.shadingMode == ShadingMode::SolidAndWire)
	{
		RenderContext wireContext = rc;
		wireContext.renderSettings.shadingMode = ShadingMode::Wire;
		UpdateTerrainSceneConstantBuffer(dc, wireContext);
		rc.graphics->DepthReadState->Bind(dc);
		rc.graphics->WireframeRasterizerState->Bind(dc);
		indexBuffer->Draw(dc, indexCount);
		UpdateTerrainSceneConstantBuffer(dc, rc);
		rc.graphics->DepthWriteState->Bind(dc);
	}
	rc.graphics->SolidRasterizerState->Bind(dc);

	Gpu::ConstantBuffer::UnbindVS(dc, 0);
	Gpu::ConstantBuffer::UnbindVS(dc, 3);
	Gpu::ConstantBuffer::UnbindVS(dc, 7);
	Gpu::ConstantBuffer::UnbindPS(dc, 0, 3);
	Gpu::ConstantBuffer::UnbindPS(dc, 4);
	Gpu::ConstantBuffer::UnbindPS(dc, 5);
	Gpu::ConstantBuffer::UnbindPS(dc, 7);

	Gpu::PixelShader::UnbindShaderResources(dc, 0);
	Gpu::PixelShader::UnbindShaderResources(dc, 1);
	Gpu::PixelShader::UnbindShaderResources(dc, 8, ShadowMapData::CascadeCount);
	Gpu::PixelShader::UnbindShaderResources(dc, 17, 3);
	Gpu::PixelShader::UnbindShaderResources(dc, 20, MaxTerrainLayers);
	Gpu::PixelShader::UnbindShaderResources(dc, 36, MaxTerrainLayers);
	Gpu::PixelShader::UnbindShaderResources(dc, 52, MaxTerrainDecals);

	Gpu::SamplerState::UnbindPS(dc, 0, 3);

	Gpu::VertexShader::Unbind(dc);
	Gpu::PixelShader::Unbind(dc);
	Gpu::VertexShader::UnbindInputLayout(dc);

	const bool paintingGrass = preview && preview->mode == 4;
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
				terrainWorld = transform->GetMatrix();
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
	PROFILE_FUNCTION();
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

	terrainSceneConstantBuffer->Update(dc, &cbScene);

	tesselationConstantBuffer->Update(dc, &tesselation_constant);

	UINT stride = sizeof(TerrainVertex);
	UINT offset = 0;

	vertexBuffer->Bind(dc, stride, 0, offset);
	indexBuffer->Bind(dc, DXGI_FORMAT_R32_UINT, 0);
	Gpu::VertexBuffer::SetTopology(dc, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	terrainVertexShader->Bind(dc);
	Gpu::HullShader::Unbind(dc);
	Gpu::DomainShader::Unbind(dc);
	Gpu::PixelShader::Unbind(dc);

	shadowMapConstantBuffer->BindVS(dc, 0);
	terrainObjectConstantBuffer->BindVS(dc, 3);
	terrainSceneConstantBuffer->BindVS(dc, 7);

	indexBuffer->Draw(dc, indexCount);

	Gpu::ConstantBuffer::UnbindVS(dc, 0);
	Gpu::ConstantBuffer::UnbindVS(dc, 3);
	Gpu::ConstantBuffer::UnbindVS(dc, 7);

	Gpu::VertexShader::Unbind(dc);
	Gpu::VertexShader::UnbindInputLayout(dc);
}

bool Terrain::AddTerrainLayer(const std::string& baseColorPath, const std::string& normalPath)
{
	if (baseColorPath.empty()) return false;

	auto& resources = ResourceManager::Instance();
	const auto type = ResourceManager::ResourceType::TerrainLayer;
	const std::string basePath = ResourceManager::PortableResourcePath(resources.ResolveEditPath(baseColorPath, type));
	const auto key = ResourceManager::ShortResourcePath(basePath, type);
	for (const auto& layer : terrainLayers)
		if (ResourceManager::ShortResourcePath(layer.baseColorPath, type) == key) return true;
	if (terrainLayers.size() >= MaxTerrainLayers)
	{
		terrainIoMessage = "地形レイヤー数の上限です。";
		return false;
	}
	TerrainLayer layer;
	layer.name = ResourcePath::Text(ResourcePath::Path(basePath).stem());
	layer.baseColorPath = basePath;
	if (!normalPath.empty()) layer.normalPath = ResourceManager::PortableResourcePath(resources.ResolveEditPath(normalPath, type));
	MipmapTexture baseTexture(layer.baseColorPath.c_str());
	layer.baseColorView = baseTexture.GetShaderResourceView();
	if (layer.normalPath.empty()) layer.normalView = defaultNormalTexture->GetShaderResourceView();
	else
	{
		MipmapTexture normalTexture(layer.normalPath.c_str());
		layer.normalView = normalTexture.GetShaderResourceView();
	}
	terrainLayers.push_back(std::move(layer));
	return true;
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

	try
	{
		filepath = ResourceManager::Instance().SaveResourceFile(filepath, [&](const std::filesystem::path& destination) {
			if (FAILED(DirectX::SaveToDDSFile(image, DirectX::DDS_FLAGS_NONE, destination.c_str())))
				throw std::runtime_error("Terrain DDS save failed.");
		});
	}
	catch (const std::exception& exception)
	{
		terrainIoMessage = exception.what();
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

	ImageData decodedImage;
	HRESULT hr = decodedImage.LoadFile(filepath);
	const auto& sourceMetadata = decodedImage.GetMetadata();
	const auto& sourceImage = decodedImage.GetImage();
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
	const auto saveColor = [](const Color& value) {
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
	root["layers"] = json::array();
	for (const TerrainLayer& layer : terrainLayers)
	{
		root["layers"].push_back({{"baseColor", layer.baseColorPath},
			{"normal", layer.normalPath}});
	}
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
		ClearDecals();
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
			grassDirty = true;
		}
		if (root.contains("clearColor"))
			terrain_texture_clear_color = loadColor(root["clearColor"], terrain_texture_clear_color);

		terrainLayers.clear();
		for (const auto& layer : root.value("layers", json::array()))
			if (!AddTerrainLayer(layer.value("baseColor", ""), layer.value("normal", ""))) return false;

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
	if (!image)
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
	PROFILE_FUNCTION();
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
	if (terrainPixels.empty()) return 0.0f;

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
	// 保存済みの変形前の高さを優先し、ナビメッシュの基準面を維持する。
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

	if (normal.LengthSquared() <= eps) return Vector3::UnitY;

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

	const Vector3 localPosition = Vector3::Transform(worldPosition, transform->GetMatrix().Invert());
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
	if (grassRemovalEffectCooldown.GetRemaining() > 0.0f) return;

	auto& effects = EffectManager::Instance();
	if (!grassRemovalEffect) grassRemovalEffect = effects.LoadEffect("feather");
	effects.PlayDetached(grassRemovalEffect, 
		 Matrix::CreateScale(0.3f) * Matrix::CreateTranslation(worldPosition));
	grassRemovalEffectCooldown.Reset(0.08f);
}

// sphereの中にある草マップを範囲内で削除する
bool Terrain::RemoveGrassInSphere(const Vector3& worldCenter, float worldRadius, LayerId sourceLayer)
{
	if (!PhysicsLayerManager::Instance().Collides(sourceLayer, Layers::Get("GrassMap"))) return false;
	if (terrainPixels.empty() || worldRadius <= 0.0f) return false;

	const Transform* transform = owner ? owner->GetTransform() : nullptr;
	if (!transform) return false;

	// 球の範囲を地形座標へ移し、調べる草マップの領域を絞る。
	const Matrix& world = transform->GetMatrix();
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

	// 草の根元から先端までの線分と球の距離で、削除対象を判定する。
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

	// 削除した位置の平均にエフェクトを出し、草のマスクを更新する。
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

	const Matrix inverseWorld = transform->GetMatrix().Invert();
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

	// 半径内の高さを滑らかに変え、周囲の草も取り除く。
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
			removedGrassPosition / static_cast<float>(removedGrassCount), transform->GetMatrix()));

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
