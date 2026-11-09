#include "Resource/ResourceManager.h"
#include <Dx11Kit/Dx11Kit.h>
#include "Rendering/Core/Graphics.h"
#include <ProfileKit/ProfileKit.h>
#include "Gameplay/Stage/Component/TerrainGrassRenderer.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

#include "Application/Time/GameTime.h"
#include "Gameplay/Camera/Camera.h"
#include "Gameplay/Stage/Component/Terrain.h"
#include "Rendering/Core/RenderContext.h"

TerrainGrassRenderer::TerrainGrassRenderer(ID3D11Device* device) : device(device)
{
	if (!device) return;
	Gpu::InputLayoutDesc elements;
	elements.AddPosition()
		.AddNormal()
		.AddTexcoord(DXGI_FORMAT_R32_FLOAT);
	vertexShader = std::make_unique<Gpu::VertexShader>(device, ResourceManager::Instance().ResolveShaderPath("Resources/Shader/TerrainGrassVS.cso").c_str(),
		elements);
	geometryShader = std::make_unique<Gpu::GeometryShader>(device, ResourceManager::Instance().ResolveShaderPath("Resources/Shader/TerrainGrassGS.cso").c_str());
	pixelShader = std::make_unique<Gpu::PixelShader>(device, ResourceManager::Instance().ResolveShaderPath("Resources/Shader/TerrainGrassPS.cso").c_str());
	constantBuffer = std::make_unique<Gpu::ConstantBuffer>(device, sizeof(Constants));
}

void TerrainGrassRenderer::Rebuild(const Terrain& terrain, const Settings& settings)
{
	PROFILE_FUNCTION();
	vertexBuffer.reset();
	tuftCount = 0;
	if (!device) return;
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

			// 緑の表示と同じ閾値で草の有無を確定する
			const float grassMask = terrain.GetGrassMaskByUV(u, v);
			if (grassMask < 0.5f) continue;

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

	const Gpu::BufferDesc desc(static_cast<UINT>(vertices.size() * sizeof(Vertex)),
		D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_IMMUTABLE);
	D3D11_SUBRESOURCE_DATA data{};
	data.pSysMem = vertices.data();
	try
	{
		vertexBuffer = std::make_unique<Gpu::VertexBuffer>(device.Get(), desc, &data);
		tuftCount = static_cast<int>(vertices.size());
	}
	catch (const std::exception&) { return; }
}

void TerrainGrassRenderer::Render(
	const RenderContext& rc, const Matrix& world, const Settings& settings,
	ID3D11ShaderResourceView* terrainDataView, float terrainSize)
{
	PROFILE_FUNCTION();
	if (tuftCount <= 0) return;
	if (!vertexBuffer || !constantBuffer) return;
	if (!rc.camera || !rc.deviceContext || !rc.graphics) return;
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
	constants.shadingMode = static_cast<float>(rc.renderSettings.shadingMode);
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
	constantBuffer->Update(rc.deviceContext, &constants);

	rc.graphics->OpaqueBlendState->Bind(rc.deviceContext);
	rc.graphics->DepthWriteState->Bind(rc.deviceContext);
	rc.graphics->SolidRasterizerState->Bind(rc.deviceContext);
	Gpu::VertexBuffer::SetTopology(rc.deviceContext, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	vertexBuffer->Bind(rc.deviceContext, stride, 0, offset);
	Gpu::IndexBuffer::Unbind(rc.deviceContext, DXGI_FORMAT_UNKNOWN);
	vertexShader->Bind(rc.deviceContext);
	geometryShader->Bind(rc.deviceContext);
	pixelShader->Bind(rc.deviceContext);
	constantBuffer->BindVS(rc.deviceContext, 0);
	constantBuffer->BindGS(rc.deviceContext, 0);
	constantBuffer->BindPS(rc.deviceContext, 0);
	Gpu::GeometryShader::SetShaderResources(rc.deviceContext, 0, terrainDataView);
	rc.graphics->PointClampSamplerState->BindGS(rc.deviceContext, 0);
	vertexBuffer->Draw(rc.deviceContext, tuftCount);

	Gpu::GeometryShader::UnbindShaderResources(rc.deviceContext, 0);
	Gpu::SamplerState::UnbindGS(rc.deviceContext, 0);
	Gpu::GeometryShader::Unbind(rc.deviceContext);
}
