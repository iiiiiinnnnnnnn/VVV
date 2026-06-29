// Terrain.cpp

#include "Terrain.h"
#include <DirectXTex.h>
#include "Actor.h"
#include "Graphics.h"
#include "GpuResourceUtils.h"
#include "LightManager.h"
#include "Input.h"
#include "Misc.h"
#include "IconsFontAwesome5.h"

Terrain::Terrain(Object* owner)
	: Component(owner)
{
	GetOwnerAsActor();

	InitializeGpuResources();
	ClearTerrainTexture();

	std::error_code error;
	std::filesystem::create_directories("Data/Terrain/Maps", error);

	AddBrushTexture("Data/Terrain/Brushes/brush_default.png");
	AddBrushTexture("Data/Terrain/Brushes/brush_pen.png");
	AddBrushTexture("Data/Terrain/Brushes/brush_square.png");
	AddBrushTexture("Data/Terrain/Brushes/brush_triangle.png");
	AddBrushTexture("Data/Terrain/Brushes/brush_manji.png");
	if (brushes.empty())
	{
		AddBrushTexture("Data/Image/bugTex.png");
	}

	LoadTerrainTexture(terrainFilePath);
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
		"Data/Shader/TerrainPrimitiveVS.cso",
		inputElementDescs,
		_countof(inputElementDescs),
		terrainInputLayout.GetAddressOf(),
		terrainVertexShader.GetAddressOf());

	GpuResourceUtils::LoadHullShader(
		device,
		"Data/Shader/TerrainPrimitiveHS.cso",
		terrainHullShader.GetAddressOf());

	GpuResourceUtils::LoadDomainShader(
		device,
		"Data/Shader/TerrainPrimitiveDS.cso",
		terrainDomainShader.GetAddressOf());

	GpuResourceUtils::LoadPixelShader(
		device,
		"Data/Shader/TerrainPrimitivePS.cso",
		terrainPixelShader.GetAddressOf());

	GpuResourceUtils::LoadComputeShader(
		device,
		"Data/Shader/TerrainColliderBuildCS.cso",
		terrainColliderBuildComputeShader.GetAddressOf());

	// レイヤー追加
	// ブレンドで違和感のない順番で追加する

	AddTerrainLayer("Data/Terrain/Layers/stone.dds", "Data/Terrain/Layers/stone_n.dds");
	AddTerrainLayer("Data/Terrain/Layers/rock.dds", "Data/Terrain/Layers/rock_n.dds");
	AddTerrainLayer("Data/Terrain/Layers/dirt.dds", "Data/Terrain/Layers/dirt_n.dds");
	AddTerrainLayer("Data/Terrain/Layers/grass.dds", "Data/Terrain/Layers/grass_n.dds");

	// エラー用
	if (terrainLayers.empty())
	{
		AddTerrainLayer("Data/Image/bugtex.dds", "Data/Image/bugtex.dds");
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

	const int vertexLineCount = gridResolution + 1;
	vertices.reserve(vertexLineCount * vertexLineCount);
	indices.reserve(gridResolution * gridResolution * 6);

	for (int z = 0; z <= gridResolution; ++z)
	{
		for (int x = 0; x <= gridResolution; ++x)
		{
			float u = static_cast<float>(x) / static_cast<float>(gridResolution);
			float v = static_cast<float>(z) / static_cast<float>(gridResolution);

			TerrainVertex vertex{};
			vertex.position = {
				(u - 0.5f) * terrainSize,
				0.0f,
				(v - 0.5f) * terrainSize
			};
			vertex.normal = Vector3::UnitY;
			vertex.texcoord = {u, v};

			vertices.push_back(vertex);
		}
	}

	for (int z = 0; z < gridResolution; ++z)
	{
		for (int x = 0; x < gridResolution; ++x)
		{
			uint32_t i0 = static_cast<uint32_t>(z * vertexLineCount + x);
			uint32_t i1 = i0 + 1;
			uint32_t i2 = i0 + static_cast<uint32_t>(vertexLineCount);
			uint32_t i3 = i2 + 1;

			indices.push_back(i0);
			indices.push_back(i2);
			indices.push_back(i1);

			indices.push_back(i1);
			indices.push_back(i2);
			indices.push_back(i3);
		}
	}

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
	for (Vector4& pixel : terrainPixels)
	{
		pixel.x = terrain_texture_clear_color.x;
		pixel.y = terrain_texture_clear_color.y;
		pixel.z = terrain_texture_clear_color.z;
		pixel.w = terrain_texture_clear_color.w;
	}

	terrainTextureDirty = true;
	is_terrain_texture_clear_color = false;
}

void Terrain::UpdateTerrainObjectConstantBuffer(ID3D11DeviceContext* dc)
{
	Actor* actor = GetOwnerAsActor();

	CbTerrainObject cbObject{};
	cbObject.world = actor->transform.matrix;
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
	const Matrix& lightViewProjection,
	const Color& shadowColor,
	float shadowBias,
	int pcfKernelSize)
{
	CbShadowMap cbShadow{};
	cbShadow.lightViewProjection = lightViewProjection;
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
	cbMaterial.metalness = std::clamp(metalness, 0.0f, 1.0f);
	cbMaterial.roughness = std::clamp(roughness, 0.0001f, 1.0f);
	cbMaterial.occlusion = std::clamp(occlusion, 0.0f, 1.0f);
	cbMaterial.occlusionStrength = std::clamp(occlusionStrength, 0.0f, 1.0f);
	cbMaterial.shadowStrength = std::clamp(shadowStrength, 0.0f, 1.0f);
	cbMaterial.useMetalnessTexture = 0;
	cbMaterial.useRoughnessTexture = 0;
	cbMaterial.useOcclusionTexture = 0;

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

	if (TerrainMeshCollider* collider = owner->GetComponent<TerrainMeshCollider>())
	{
		if (collider->NeedsGpuRebuild())
		{
			collider->RebuildFromTerrain();
			pendingColliderRebuild = false;
		}
	}

	UpdateTerrainObjectConstantBuffer(dc);
	UpdateTerrainSceneConstantBuffer(dc, rc);
	UpdateShadowConstantBuffer(
		dc,
		rc.shadowMapData.lightViewProjection,
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
		rc.renderState->GetRasterizerState(
			use_wire ? RasterizerState::WireCullNone : RasterizerState::SolidCullNone));

	UINT stride = sizeof(TerrainVertex);
	UINT offset = 0;
	ID3D11Buffer* vertexBuffers[] = {vertexBuffer.Get()};

	dc->IASetInputLayout(terrainInputLayout.Get());
	dc->IASetVertexBuffers(0, 1, vertexBuffers, &stride, &offset);
	dc->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);

	dc->VSSetShader(terrainVertexShader.Get(), nullptr, 0);
	dc->HSSetShader(terrainHullShader.Get(), nullptr, 0);
	dc->DSSetShader(terrainDomainShader.Get(), nullptr, 0);
	dc->PSSetShader(terrainPixelShader.Get(), nullptr, 0);

	ID3D11Buffer* objectCb = terrainObjectConstantBuffer.Get();
	ID3D11Buffer* tessCb = tesselationConstantBuffer.Get();
	ID3D11Buffer* shadowCb = shadowMapConstantBuffer.Get();
	ID3D11Buffer* sceneCb = terrainSceneConstantBuffer.Get();
	ID3D11Buffer* layerCb = terrainLayerConstantBuffer.Get();

	ID3D11Buffer* dsTerrainCbs[] =
	{
		tesselationConstantBuffer.Get(),
		terrainObjectConstantBuffer.Get(),
	};

	ID3D11Buffer* psPbrCbs[] =
	{
		shadowMapConstantBuffer.Get(),
		materialConstantBuffer.Get(),
		tesselationConstantBuffer.Get(),
	};

	dc->VSSetConstantBuffers(3, 1, &objectCb);
	dc->HSSetConstantBuffers(2, 1, &tessCb);
	dc->DSSetConstantBuffers(0, 1, &shadowCb);
	dc->DSSetConstantBuffers(2, _countof(dsTerrainCbs), dsTerrainCbs);
	dc->DSSetConstantBuffers(7, 1, &sceneCb);
	dc->PSSetConstantBuffers(0, _countof(psPbrCbs), psPbrCbs);
	dc->PSSetConstantBuffers(4, 1, &layerCb);
	dc->PSSetConstantBuffers(7, 1, &sceneCb);

	ID3D11ShaderResourceView* terrainSrv = terrainTextureShaderResourceView.Get();
	dc->DSSetShaderResources(0, 1, &terrainSrv);
	dc->PSSetShaderResources(0, 1, &terrainSrv);

	dc->PSSetShaderResources(8, 1, &rc.shadowMapData.shadowMap);

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

	dc->DSSetSamplers(0, _countof(samplerStates), samplerStates);
	dc->PSSetSamplers(0, _countof(samplerStates), samplerStates);

	dc->DrawIndexed(indexCount, 0, 0);

	ID3D11Buffer* nullCb = nullptr;
	ID3D11Buffer* nullCbs2[] = {nullptr, nullptr};
	ID3D11Buffer* nullCbs3[] = {nullptr, nullptr, nullptr};

	dc->VSSetConstantBuffers(3, 1, &nullCb);
	dc->HSSetConstantBuffers(2, 1, &nullCb);
	dc->DSSetConstantBuffers(0, 1, &nullCb);
	dc->DSSetConstantBuffers(2, _countof(nullCbs2), nullCbs2);
	dc->DSSetConstantBuffers(7, 1, &nullCb);
	dc->PSSetConstantBuffers(0, _countof(nullCbs3), nullCbs3);
	dc->PSSetConstantBuffers(4, 1, &nullCb);
	dc->PSSetConstantBuffers(7, 1, &nullCb);

	ID3D11ShaderResourceView* nullSrv = nullptr;
	ID3D11ShaderResourceView* nullSrvs3[] = {nullptr, nullptr, nullptr};
	ID3D11ShaderResourceView* nullTerrainSrvs[MaxTerrainLayers] = {};

	dc->DSSetShaderResources(0, 1, &nullSrv);
	dc->PSSetShaderResources(0, 1, &nullSrv);
	dc->PSSetShaderResources(8, 1, &nullSrv);
	dc->PSSetShaderResources(17, _countof(nullSrvs3), nullSrvs3);
	dc->PSSetShaderResources(20, MaxTerrainLayers, nullTerrainSrvs);
	dc->PSSetShaderResources(36, MaxTerrainLayers, nullTerrainSrvs);

	ID3D11SamplerState* nullSamplers3[] = {nullptr, nullptr, nullptr};
	dc->DSSetSamplers(0, _countof(nullSamplers3), nullSamplers3);
	dc->PSSetSamplers(0, _countof(nullSamplers3), nullSamplers3);

	dc->VSSetShader(nullptr, nullptr, 0);
	dc->HSSetShader(nullptr, nullptr, 0);
	dc->DSSetShader(nullptr, nullptr, 0);
	dc->PSSetShader(nullptr, nullptr, 0);
	dc->IASetInputLayout(nullptr);
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
	UpdateTerrainObjectConstantBuffer(dc);
	UpdateShadowConstantBuffer(
		dc,
		lightViewProjection,
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
	dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);

	dc->VSSetShader(terrainVertexShader.Get(), nullptr, 0);
	dc->HSSetShader(terrainHullShader.Get(), nullptr, 0);
	dc->DSSetShader(terrainDomainShader.Get(), nullptr, 0);
	dc->PSSetShader(nullptr, nullptr, 0);

	ID3D11Buffer* objectCb = terrainObjectConstantBuffer.Get();
	ID3D11Buffer* tessCb = tesselationConstantBuffer.Get();
	ID3D11Buffer* shadowCb = shadowMapConstantBuffer.Get();
	ID3D11Buffer* sceneCb = terrainSceneConstantBuffer.Get();

	ID3D11Buffer* dsTerrainCbs[] =
	{
		tesselationConstantBuffer.Get(),
		terrainObjectConstantBuffer.Get(),
	};

	dc->VSSetConstantBuffers(3, 1, &objectCb);
	dc->HSSetConstantBuffers(2, 1, &tessCb);
	dc->DSSetConstantBuffers(0, 1, &shadowCb);
	dc->DSSetConstantBuffers(2, _countof(dsTerrainCbs), dsTerrainCbs);
	dc->DSSetConstantBuffers(7, 1, &sceneCb);

	ID3D11ShaderResourceView* terrainSrv = terrainTextureShaderResourceView.Get();
	dc->DSSetShaderResources(0, 1, &terrainSrv);

	ID3D11SamplerState* pointSampler =
		Game::Graphics::Instance().GetRenderState()->GetSamplerState(SamplerState::PointClamp);
	dc->DSSetSamplers(0, 1, &pointSampler);

	dc->DrawIndexed(indexCount, 0, 0);

	ID3D11Buffer* nullCb = nullptr;
	ID3D11Buffer* nullCbs2[] = {nullptr, nullptr};
	ID3D11ShaderResourceView* nullSrv = nullptr;
	ID3D11SamplerState* nullSampler = nullptr;

	dc->VSSetConstantBuffers(3, 1, &nullCb);
	dc->HSSetConstantBuffers(2, 1, &nullCb);
	dc->DSSetConstantBuffers(0, 1, &nullCb);
	dc->DSSetConstantBuffers(2, _countof(nullCbs2), nullCbs2);
	dc->DSSetConstantBuffers(7, 1, &nullCb);
	dc->DSSetShaderResources(0, 1, &nullSrv);
	dc->DSSetSamplers(0, 1, &nullSampler);

	dc->VSSetShader(nullptr, nullptr, 0);
	dc->HSSetShader(nullptr, nullptr, 0);
	dc->DSSetShader(nullptr, nullptr, 0);
	dc->IASetInputLayout(nullptr);
}

void Terrain::PaintByMouse(const RenderContext& rc)
{
	if (!use_brush)
	{
		return;
	}

	if (!Game::Input::IsFocusedWindow())
	{
		return;
	}

	ImGuiIO& io = ImGui::GetIO();
	if (io.KeyAlt)
	{
		return;
	}

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

	float ndcX = (2.0f * static_cast<float>(mouse.GetPositionX()) / screenWidth) - 1.0f;
	float ndcY = 1.0f - (2.0f * static_cast<float>(mouse.GetPositionY()) / screenHeight);

	Matrix viewProjection = rc.camera->GetView() * rc.camera->GetProjection();
	Matrix invViewProjection = viewProjection.Invert();

	Vector3 nearPoint = Vector3::Transform(Vector3(ndcX, ndcY, 0.0f), invViewProjection);
	Vector3 farPoint = Vector3::Transform(Vector3(ndcX, ndcY, 1.0f), invViewProjection);

	Vector3 rayOriginWorld = nearPoint;
	Vector3 rayDirectionWorld = farPoint - nearPoint;
	rayDirectionWorld.Normalize();

	Actor* actor = GetOwnerAsActor();
	Matrix invWorld = actor->transform.matrix.Invert();

	Vector3 rayOriginLocal = Vector3::Transform(rayOriginWorld, invWorld);
	Vector3 rayDirectionLocal = Vector3::TransformNormal(rayDirectionWorld, invWorld);
	rayDirectionLocal.Normalize();

	if (fabsf(rayDirectionLocal.y) < eps)
	{
		return false;
	}

	float t = -rayOriginLocal.y / rayDirectionLocal.y;
	if (t < 0.0f)
	{
		return false;
	}

	Vector3 hitLocal = rayOriginLocal + rayDirectionLocal * t;

	float u = hitLocal.x / terrainSize + 0.5f;
	float v = hitLocal.z / terrainSize + 0.5f;

	if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f)
	{
		return false;
	}

	outU = u;
	outV = v;

	return true;
}

void Terrain::ApplyBrush(float u, float v, float heightSign)
{
	const TerrainBrush* brush = GetCurrentBrush();
	if (brush == nullptr || brush_size <= 0)
	{
		return;
	}

	const int centerX = static_cast<int>(u * static_cast<float>(TerrainTextureWidth - 1));
	const int centerY = static_cast<int>(v * static_cast<float>(TerrainTextureHeight - 1));
	const int radius = brush_size;

	const int x0 = max(centerX - radius, 0);
	const int y0 = max(centerY - radius, 0);
	const int x1 = min(centerX + radius, TerrainTextureWidth - 1);
	const int y1 = min(centerY + radius, TerrainTextureHeight - 1);
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
		}
	}

	terrainTextureDirty = true;
	if (brushMode == BrushMode::RaiseLower || brushMode == BrushMode::SetHeight)
	{
		pendingColliderRebuild = true;
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
			minimumAlpha = min(minimumAlpha, pixel[3]);
			maximumAlpha = max(maximumAlpha, pixel[3]);
		}
	}

	const bool useAlphaMask =
		static_cast<int>(maximumAlpha) -
		static_cast<int>(minimumAlpha) > 4;

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
	const int x1 = min(x0 + 1, brush->width - 1);
	const int y1 = min(y0 + 1, brush->height - 1);

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

	const std::filesystem::path basePath =
		std::filesystem::path(baseColorPath).lexically_normal();
	const std::filesystem::path normalMapPath =
		std::filesystem::path(normalPath).lexically_normal();

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

	ID3D11Device* device = Game::Graphics::Instance().GetDevice();
	HRESULT hr = GpuResourceUtils::LoadTexture(
		device,
		layer.baseColorPath.c_str(),
		layer.baseColorView.GetAddressOf());

	if (FAILED(hr))
	{
		terrainIoMessage = "Terrain layer base color load failed: " + layer.baseColorPath;
		return false;
	}

	hr = GpuResourceUtils::LoadTexture(
		device,
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

	terrainFilePath = filepath.generic_string();
	terrainTextureDirty = true;
	is_terrain_texture_clear_color = false;
	pendingColliderRebuild = true;

	terrainIoMessage = "Terrain loaded. Collider rebuild is pending.";

	return true;
}

std::filesystem::path Terrain::GetColliderVertexPath() const
{
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
	ID3D11Device* device = Game::Graphics::Instance().GetDevice();
	ID3D11DeviceContext* dc = Game::Graphics::Instance().GetDeviceContext();
	if (!device || !dc || !terrainColliderBuildComputeShader || !terrainTextureShaderResourceView)
	{
		terrainIoMessage = "Terrain collider GPU bake failed: GPU resource is missing.";
		return false;
	}

	minX = std::clamp(minX, 0.0f, 1.0f);
	maxX = std::clamp(maxX, 0.0f, 1.0f);
	minZ = std::clamp(minZ, 0.0f, 1.0f);
	maxZ = std::clamp(maxZ, 0.0f, 1.0f);

	if (minX > maxX) std::swap(minX, maxX);
	if (minZ > maxZ) std::swap(minZ, maxZ);

	const int tessellationSegments = max(
		static_cast<int>(std::ceil(max(
			tesselation_constant.edge_factor,
			tesselation_constant.inner_factor))),
		1);
	const int totalSegmentCountX = max(gridResolution * tessellationSegments, 1);
	const int totalSegmentCountZ = max(gridResolution * tessellationSegments, 1);

	const int minGridX = std::clamp(static_cast<int>(std::floor(minX * totalSegmentCountX)), 0, totalSegmentCountX - 1);
	const int maxGridX = std::clamp(static_cast<int>(std::ceil(maxX * totalSegmentCountX)), minGridX + 1, totalSegmentCountX);
	const int minGridZ = std::clamp(static_cast<int>(std::floor(minZ * totalSegmentCountZ)), 0, totalSegmentCountZ - 1);
	const int maxGridZ = std::clamp(static_cast<int>(std::ceil(maxZ * totalSegmentCountZ)), minGridZ + 1, totalSegmentCountZ);
	const int segmentCountX = maxGridX - minGridX;
	const int segmentCountZ = maxGridZ - minGridZ;
	const int vertexLineCount = segmentCountX + 1;
	const size_t vertexCount =
		static_cast<size_t>(segmentCountX + 1) *
		static_cast<size_t>(segmentCountZ + 1);

	if (vertexCount == 0 || vertexCount > static_cast<size_t>(UINT_MAX))
	{
		terrainIoMessage = "Terrain collider GPU bake failed: vertex count is invalid.";
		return false;
	}

	CbTerrainColliderBuild cb{};
	cb.terrainSize = terrainSize;
	cb.heightMapTexelSize = 1.0f / static_cast<float>(TerrainTextureWidth);
	cb.heightScaler = tesselation_constant.height_scaler;
	cb.minGridX = minGridX;
	cb.minGridZ = minGridZ;
	cb.segmentCountX = segmentCountX;
	cb.segmentCountZ = segmentCountZ;
	cb.totalSegmentCountX = totalSegmentCountX;
	cb.totalSegmentCountZ = totalSegmentCountZ;
	cb.vertexLineCount = vertexLineCount;

	dc->UpdateSubresource(terrainColliderBuildConstantBuffer.Get(), 0, nullptr, &cb, 0, 0);
	UploadTerrainTexture(dc);

	D3D11_BUFFER_DESC outputDesc{};
	outputDesc.ByteWidth = static_cast<UINT>(sizeof(Vector4) * vertexCount);
	outputDesc.Usage = D3D11_USAGE_DEFAULT;
	outputDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	outputDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	outputDesc.StructureByteStride = sizeof(Vector4);

	Microsoft::WRL::ComPtr<ID3D11Buffer> outputBuffer;
	HRESULT hr = device->CreateBuffer(&outputDesc, nullptr, outputBuffer.GetAddressOf());
	if (FAILED(hr))
	{
		terrainIoMessage = "Terrain collider GPU bake failed: output buffer creation failed.";
		return false;
	}

	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.Buffer.NumElements = static_cast<UINT>(vertexCount);

	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> outputUav;
	hr = device->CreateUnorderedAccessView(outputBuffer.Get(), &uavDesc, outputUav.GetAddressOf());
	if (FAILED(hr))
	{
		terrainIoMessage = "Terrain collider GPU bake failed: output UAV creation failed.";
		return false;
	}

	D3D11_BUFFER_DESC stagingDesc = outputDesc;
	stagingDesc.Usage = D3D11_USAGE_STAGING;
	stagingDesc.BindFlags = 0;
	stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	stagingDesc.MiscFlags = 0;
	stagingDesc.StructureByteStride = 0;

	Microsoft::WRL::ComPtr<ID3D11Buffer> stagingBuffer;
	hr = device->CreateBuffer(&stagingDesc, nullptr, stagingBuffer.GetAddressOf());
	if (FAILED(hr))
	{
		terrainIoMessage = "Terrain collider GPU bake failed: staging buffer creation failed.";
		return false;
	}

	Microsoft::WRL::ComPtr<ID3D11ComputeShader> prevShader;
	Microsoft::WRL::ComPtr<ID3D11Buffer> prevConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> prevShaderResourceView;
	Microsoft::WRL::ComPtr<ID3D11SamplerState> prevSampler;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> prevUnorderedAccessView;

	dc->CSGetShader(prevShader.GetAddressOf(), nullptr, nullptr);
	dc->CSGetConstantBuffers(0, 1, prevConstantBuffer.GetAddressOf());
	dc->CSGetShaderResources(0, 1, prevShaderResourceView.GetAddressOf());
	dc->CSGetSamplers(0, 1, prevSampler.GetAddressOf());
	dc->CSGetUnorderedAccessViews(0, 1, prevUnorderedAccessView.GetAddressOf());

	ID3D11UnorderedAccessView* uavs[] = {outputUav.Get()};
	ID3D11ShaderResourceView* srvs[] = {terrainTextureShaderResourceView.Get()};
	ID3D11SamplerState* samplers[] =
	{
		Game::Graphics::Instance().GetRenderState()->GetSamplerState(SamplerState::PointClamp)
	};
	ID3D11Buffer* cbs[] = {terrainColliderBuildConstantBuffer.Get()};

	dc->CSSetShader(terrainColliderBuildComputeShader.Get(), nullptr, 0);
	dc->CSSetConstantBuffers(0, 1, cbs);
	dc->CSSetShaderResources(0, 1, srvs);
	dc->CSSetSamplers(0, 1, samplers);
	dc->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

	const UINT groupCount = static_cast<UINT>((vertexCount + 255) / 256);
	dc->Dispatch(groupCount, 1, 1);

	ID3D11UnorderedAccessView* nullUav = nullptr;
	ID3D11ShaderResourceView* nullSrv = nullptr;
	ID3D11SamplerState* nullSampler = nullptr;
	ID3D11Buffer* nullCb = nullptr;
	dc->CSSetUnorderedAccessViews(0, 1, &nullUav, nullptr);
	dc->CSSetShaderResources(0, 1, &nullSrv);
	dc->CSSetSamplers(0, 1, &nullSampler);
	dc->CSSetConstantBuffers(0, 1, &nullCb);
	dc->CSSetShader(nullptr, nullptr, 0);

	dc->CopyResource(stagingBuffer.Get(), outputBuffer.Get());

	ID3D11ComputeShader* restoreShader = prevShader.Get();
	ID3D11Buffer* restoreConstantBuffer = prevConstantBuffer.Get();
	ID3D11ShaderResourceView* restoreShaderResourceView = prevShaderResourceView.Get();
	ID3D11SamplerState* restoreSampler = prevSampler.Get();
	ID3D11UnorderedAccessView* restoreUnorderedAccessView = prevUnorderedAccessView.Get();
	dc->CSSetShader(restoreShader, nullptr, 0);
	dc->CSSetConstantBuffers(0, 1, &restoreConstantBuffer);
	dc->CSSetShaderResources(0, 1, &restoreShaderResourceView);
	dc->CSSetSamplers(0, 1, &restoreSampler);
	dc->CSSetUnorderedAccessViews(0, 1, &restoreUnorderedAccessView, nullptr);

	D3D11_MAPPED_SUBRESOURCE mapped{};
	hr = dc->Map(stagingBuffer.Get(), 0, D3D11_MAP_READ, 0, &mapped);
	if (FAILED(hr))
	{
		terrainIoMessage = "Terrain collider GPU bake failed: staging map failed.";
		return false;
	}

	const Vector4* gpuVertices = static_cast<const Vector4*>(mapped.pData);
	vertices.resize(vertexCount);
	for (size_t i = 0; i < vertexCount; ++i)
	{
		vertices[i] = Vector3(gpuVertices[i].x, gpuVertices[i].y, gpuVertices[i].z);
	}
	dc->Unmap(stagingBuffer.Get(), 0);

	indices.clear();
	indices.reserve(
		static_cast<size_t>(segmentCountX) *
		static_cast<size_t>(segmentCountZ) * 6);

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

	terrainIoMessage = "Terrain collider GPU baked.";
	return true;
}

void Terrain::RebuildTerrainCollider()
{
	TerrainMeshCollider* collider = owner->GetComponent<TerrainMeshCollider>();
	if (collider == nullptr)
	{
		pendingColliderRebuild = true;
		return;
	}

	collider->RebuildFromTerrain();
	pendingColliderRebuild = false;
}

void Terrain::DrawTerrainLayerGUI()
{
	if (!ImGui::TreeNode("Terrain Paint Layers"))
	{
		return;
	}

	ImGui::Text("Selected: %s",
		terrainLayers.empty()
			? "None"
			: terrainLayers[currentTerrainLayerIndex].name.c_str());

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

	ImGui::Text("Layers: %d / %d",
		static_cast<int>(terrainLayers.size()),
		MaxTerrainLayers);

	ImGui::TreePop();
}

void Terrain::DrawBrushGUI()
{
	if (!ImGui::TreeNode("Brush Texture"))
	{
		return;
	}

	const TerrainBrush* currentBrush = GetCurrentBrush();
	ImGui::Text("Selected: %s",
		currentBrush != nullptr ? currentBrush->name.c_str() : "None");

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

	ImGui::Checkbox("invert brush mask", &invertBrushMask);

	ImGui::Text("Brush count: %d", static_cast<int>(brushes.size()));
	ImGui::TreePop();
}

void Terrain::DrawGUI()
{
	if (!ImGui::TreeNode(ICON_FA_MOUNTAIN " Terrain"))
	{
		return;
	}

	if (ImGui::TreeNode("Terrain File"))
	{
		ImGui::InputText("terrain file", &terrainFilePath);

		if (ImGui::Button("Save Terrain"))
		{
			SaveTerrainTexture(terrainFilePath);
		}

		ImGui::SameLine();

		if (ImGui::Button("Load Terrain"))
		{
			LoadTerrainTexture(terrainFilePath);
		}

		ImGui::SameLine();

		if (ImGui::Button("Rebuild Collider"))
		{
			pendingColliderRebuild = true;
			RebuildTerrainCollider();
		}

		ImGui::Text("Collider: %s", pendingColliderRebuild ? "dirty" : "clean");

		if (!terrainIoMessage.empty())
		{
			ImGui::TextWrapped("%s", terrainIoMessage.c_str());
		}

		ImGui::TextWrapped("Terrain maps are saved as 1024 x 1024 float DDS files.");
		ImGui::TreePop();
	}

	DrawBrushGUI();
	DrawTerrainLayerGUI();

	if (ImGui::TreeNode("Terrain PBR"))
	{
		ImGui::ColorEdit4("base color", &baseColor.x);
		ImGui::ColorEdit4("emissive color", &emissiveColor.x);
		ImGui::SliderFloat("metalness", &metalness, 0.0f, 1.0f);
		ImGui::SliderFloat("roughness", &roughness, 0.0001f, 1.0f);
		ImGui::SliderFloat("occlusion", &occlusion, 0.0f, 1.0f);
		ImGui::SliderFloat("occlusion strength", &occlusionStrength, 0.0f, 1.0f);
		ImGui::SliderFloat("shadow strength", &shadowStrength, 0.0f, 1.0f);
		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Terrain Data"))
	{
		if (ImGui::Button("terrain texture clear"))
		{
			ClearTerrainTexture();
			pendingColliderRebuild = true;
		}

		ImGui::Text("R = height, G = paint layer");
		ImGui::DragFloat4("terrain data clear RGBA", &terrain_texture_clear_color.x, 0.01f);

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

	if (ImGui::TreeNode("Terrain Brush"))
	{
		ImGui::Checkbox("use brush", &use_brush);

		int brushModeIndex = static_cast<int>(brushMode);
		const char* brushModeItems[] =
		{
			"Raise/Lower",
			"Set Height",
			"Paint",
		};

		if (ImGui::Combo("brush mode", &brushModeIndex, brushModeItems, _countof(brushModeItems)))
		{
			brushMode = static_cast<BrushMode>(brushModeIndex);
		}

		ImGui::SliderInt("brush size", &brush_size, 1, 256);
		if (brushMode == BrushMode::RaiseLower)
		{
			ImGui::DragFloat("raise/lower strength", &heightBrushStrength, 0.001f, 0.0f, 1.0f);
			ImGui::Text("Left drag: raise");
			ImGui::Text("Shift + left drag: lower");
		}
		else if (brushMode == BrushMode::SetHeight)
		{
			ImGui::DragFloat("set height", &setHeightValue, 0.001f, -10.0f, 10.0f);
			ImGui::Text("Left drag: set height");
		}
		else if (brushMode == BrushMode::Paint)
		{
			ImGui::DragFloat("paint opacity", &paintOpacity, 0.001f, 0.0f, 1.0f);
			ImGui::Text("Left drag: paint");
		}
		ImGui::Text("Alt + left drag: camera orbit only");

		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Terrain Rendering"))
	{
		float newTerrainSize = terrainSize;
		if (ImGui::DragFloat("terrain size", &newTerrainSize, 1.0f, 1.0f, 10000.0f))
		{
			terrainSize = newTerrainSize;
			CreateGridMesh(Game::Graphics::Instance().GetDevice());
			pendingColliderRebuild = true;
			RebuildTerrainCollider();
		}

		int newGridResolution = gridResolution;
		if (ImGui::DragInt("grid resolution", &newGridResolution, 1, 1, 256))
		{
			gridResolution = newGridResolution;
			CreateGridMesh(Game::Graphics::Instance().GetDevice());
		}

		ImGui::Checkbox("wire", &use_wire);
		ImGui::SliderFloat("edge", &tesselation_constant.edge_factor, 1.0f, 16.0f);
		ImGui::SliderFloat("inner", &tesselation_constant.inner_factor, 1.0f, 16.0f);
		if (ImGui::SliderFloat("height scaler", &tesselation_constant.height_scaler, -200.0f, 200.0f))
		{
			pendingColliderRebuild = true;
			RebuildTerrainCollider();
		}
		ImGui::SliderFloat("tilling scale", &tesselation_constant.tilling_scale, 1.0f, 300.0f);

		ImGui::TreePop();
	}

	ImGui::TreePop();
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
