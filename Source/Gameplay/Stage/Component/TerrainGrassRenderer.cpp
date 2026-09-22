#include "Gameplay/Stage/Component/TerrainGrassRenderer.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

#include "Application/Time/GameTime.h"
#include "Gameplay/Camera/Camera.h"
#include "Gameplay/Stage/Component/Terrain.h"
#include "Rendering/Core/RenderContext.h"
#include "Resource/GpuResourceUtils.h"

TerrainGrassRenderer::TerrainGrassRenderer(ID3D11Device* device) : device(device)
{
	if (!device) return;
	const D3D11_INPUT_ELEMENT_DESC elements[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT,
		 D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT,
		 D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT,
		 D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
	GpuResourceUtils::LoadVertexShader(device, "Resources/Shader/TerrainGrassVS.cso",
		elements, _countof(elements), inputLayout.GetAddressOf(), vertexShader.GetAddressOf());
	GpuResourceUtils::LoadGeometryShader(
		device, "Resources/Shader/TerrainGrassGS.cso", geometryShader.GetAddressOf());
	GpuResourceUtils::LoadPixelShader(
		device, "Resources/Shader/TerrainGrassPS.cso", pixelShader.GetAddressOf());
	GpuResourceUtils::CreateConstantBuffer(device, sizeof(Constants), constantBuffer.GetAddressOf());
}

void TerrainGrassRenderer::Rebuild(const Terrain& terrain, const Settings& settings)
{
	vertexBuffer.Reset();
	tuftCount = 0;
	if (!device) return;
	if (!settings.enabled) return;
	if (settings.density <= 0.0f) return;

	const float terrainSize = terrain.GetTerrainSize();
	const float terrainArea = terrainSize * terrainSize;
	// UIの密度値を1平方メートル当たりの株数へ変換する
	constexpr float densityToTuftsPerSquareMeter = 0.1f;
	const float tuftsPerSquareMeter = settings.density * densityToTuftsPerSquareMeter;
	const float requestedTuftCount = terrainArea * tuftsPerSquareMeter;
	const float requestedAxisCount = std::sqrt(requestedTuftCount);
	const int axisCount =
		std::max(1, static_cast<int>(std::ceil(requestedAxisCount)));
	std::mt19937 random(0x47524153u);
	std::uniform_real_distribution<float> jitter(-0.42f, 0.42f);
	std::uniform_real_distribution<float> variation(0.0f, 1.0f);
	std::vector<Vertex> vertices;
	vertices.reserve(static_cast<size_t>(axisCount) * static_cast<size_t>(axisCount));
	for (int z = 0; z < axisCount; ++z)
	{
		for (int x = 0; x < axisCount; ++x)
		{
			const float randomX = jitter(random);
			const float randomZ = jitter(random);
			const float gridU = (static_cast<float>(x) + 0.5f + randomX) / axisCount;
			const float gridV = (static_cast<float>(z) + 0.5f + randomZ) / axisCount;
			const float u = std::clamp(gridU, 0.0f, 1.0f);
			const float v = std::clamp(gridV, 0.0f, 1.0f);

			// 専用マスクの濃さをそのまま草の生える確率に使う
			const float grassMask = terrain.GetGrassMaskByUV(u, v);
			if (variation(random) > grassMask) continue;

			const float localX = (u - 0.5f) * terrainSize;
			const float localZ = (v - 0.5f) * terrainSize;
			const float surfaceHeight = terrain.GetSurfaceHeightByUV(u, v);

			Vertex vertex;
			vertex.position = {localX, surfaceHeight + 0.015f, localZ};
			vertex.normal = terrain.GetSurfaceNormalByUV(u, v);
			vertex.random = variation(random);
			vertices.push_back(vertex);
		}
	}
	if (vertices.empty()) return;

	D3D11_BUFFER_DESC desc{};
	desc.ByteWidth = static_cast<UINT>(vertices.size() * sizeof(Vertex));
	desc.Usage = D3D11_USAGE_IMMUTABLE;
	desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA data{};
	data.pSysMem = vertices.data();
	if (SUCCEEDED(device->CreateBuffer(&desc, &data, vertexBuffer.GetAddressOf())))
		tuftCount = static_cast<int>(vertices.size());
}

void TerrainGrassRenderer::Render(
	const RenderContext& rc, const Matrix& world, const Settings& settings,
	ID3D11ShaderResourceView* terrainDataView, float terrainSize)
{
	if (!settings.enabled) return;
	if (tuftCount <= 0) return;
	if (!vertexBuffer || !constantBuffer) return;
	if (!rc.camera || !rc.deviceContext || !rc.renderState) return;
	Constants constants{};
	constants.world = world;
	const Matrix view = rc.camera->GetView();
	const Matrix projection = rc.camera->GetProjection();
	constants.viewProjection = view * projection;
	const Vector3 eye = rc.camera->GetEye();
	constants.cameraPosition = eye;
	constants.time = Game::Time::time;
	constants.width = settings.width;
	constants.height = settings.height;
	constants.windStrength = settings.windStrength;
	constants.windSpeed = settings.windSpeed;
	constants.sizeVariation = settings.sizeVariation;
	constants.drawDistance = settings.drawDistance;
	constants.terrainSize = terrainSize;
	constants.tint = settings.tint;
	constants.fogColor = rc.renderSettings.distanceFogColor;
	float fogEnabled = 0.0f;
	if (rc.renderSettings.distanceFogEnabled)
	{
		fogEnabled = 1.0f;
	}
	constants.fogStart = rc.renderSettings.distanceFogStart;
	constants.fogEnd = rc.renderSettings.distanceFogEnd;
	constants.fogStrength = rc.renderSettings.distanceFogStrength;
	constants.fogEnabled = fogEnabled;
	rc.deviceContext->UpdateSubresource(constantBuffer.Get(), 0, nullptr, &constants, 0, 0);

	rc.deviceContext->OMSetBlendState(
		rc.renderState->GetBlendState(BlendState::Opaque), nullptr, 0xffffffff);
	rc.deviceContext->OMSetDepthStencilState(
		rc.renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
	rc.deviceContext->RSSetState(
		rc.renderState->GetRasterizerState(RasterizerState::SolidCullNone));
	rc.deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
	rc.deviceContext->IASetInputLayout(inputLayout.Get());
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	ID3D11Buffer* vertex = vertexBuffer.Get();
	rc.deviceContext->IASetVertexBuffers(0, 1, &vertex, &stride, &offset);
	rc.deviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
	rc.deviceContext->VSSetShader(vertexShader.Get(), nullptr, 0);
	rc.deviceContext->GSSetShader(geometryShader.Get(), nullptr, 0);
	rc.deviceContext->PSSetShader(pixelShader.Get(), nullptr, 0);
	ID3D11Buffer* constant = constantBuffer.Get();
	rc.deviceContext->VSSetConstantBuffers(0, 1, &constant);
	rc.deviceContext->GSSetConstantBuffers(0, 1, &constant);
	rc.deviceContext->PSSetConstantBuffers(0, 1, &constant);
	rc.deviceContext->GSSetShaderResources(0, 1, &terrainDataView);
	ID3D11SamplerState* terrainSampler =
		rc.renderState->GetSamplerState(SamplerState::PointClamp);
	rc.deviceContext->GSSetSamplers(0, 1, &terrainSampler);
	rc.deviceContext->Draw(tuftCount, 0);

	ID3D11ShaderResourceView* nullView = nullptr;
	ID3D11SamplerState* nullSampler = nullptr;
	rc.deviceContext->GSSetShaderResources(0, 1, &nullView);
	rc.deviceContext->GSSetSamplers(0, 1, &nullSampler);
	rc.deviceContext->GSSetShader(nullptr, nullptr, 0);
}
