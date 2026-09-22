// WaterRenderer.cpp
#include "Rendering/Renderer/WaterRenderer.h"

#include <algorithm>
#include <vector>

#include "Application/SettingsAndDebug/DebugUtil.h"
#include "Application/Time/GameTime.h"
#include "Core/Object/Object.h"
#include "Gameplay/Camera/Camera.h"
#include "Gameplay/Lighting/LightManager.h"
#include "Rendering/Core/Graphics.h"
#include "Rendering/Core/RenderContext.h"
#include "Resource/GpuResourceUtils.h"
#include "Resource/ResourceManager.h"
#include "Resource/Texture.h"

WaterRenderer::WaterRenderer(Object* owner) : Component(owner)
{
	static_assert(sizeof(ConstantBuffer) % 16 == 0);

	ID3D11Device* device = Game::Graphics::Instance().GetDevice();
	if (!device) return;

	const D3D11_INPUT_ELEMENT_DESC elements[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT,
		 D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT,
		 D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
	GpuResourceUtils::LoadVertexShader(device, "Resources/Shader/WaterSurfaceVS.cso",
		elements, _countof(elements), inputLayout.GetAddressOf(), vertexShader.GetAddressOf());
	GpuResourceUtils::LoadPixelShader(
		device, "Resources/Shader/WaterSurfacePS.cso", pixelShader.GetAddressOf());
	GpuResourceUtils::CreateConstantBuffer(
		device, sizeof(ConstantBuffer), constantBuffer.GetAddressOf());
	normalTexture = ResourceManager::Instance().LoadTexture(
		"Resources/Texture/Water/water_normal.png");
	heightTexture = ResourceManager::Instance().LoadTexture(
		"Resources/Texture/Water/water_height.png");
	foamTexture = ResourceManager::Instance().LoadTexture(
		"Resources/Texture/Water/foam.png");

	// 1枚板では波の頂点変形ができないため、水面を格子状に分割する
	constexpr int divisionCount = 96;
	std::vector<Vertex> vertices;
	vertices.reserve(divisionCount * divisionCount * 6);
	for (int z = 0; z < divisionCount; ++z)
	{
		const float topRate = static_cast<float>(z) / static_cast<float>(divisionCount);
		const float bottomRate = static_cast<float>(z + 1) / static_cast<float>(divisionCount);
		for (int x = 0; x < divisionCount; ++x)
		{
			const float leftRate = static_cast<float>(x) / static_cast<float>(divisionCount);
			const float rightRate = static_cast<float>(x + 1) / static_cast<float>(divisionCount);

			const Vertex topLeft = {{leftRate - 0.5f, 0.0f, topRate - 0.5f},
				{leftRate, topRate}};
			const Vertex topRight = {{rightRate - 0.5f, 0.0f, topRate - 0.5f},
				{rightRate, topRate}};
			const Vertex bottomLeft = {{leftRate - 0.5f, 0.0f, bottomRate - 0.5f},
				{leftRate, bottomRate}};
			const Vertex bottomRight = {{rightRate - 0.5f, 0.0f, bottomRate - 0.5f},
				{rightRate, bottomRate}};

			vertices.push_back(topLeft);
			vertices.push_back(topRight);
			vertices.push_back(bottomLeft);
			vertices.push_back(bottomLeft);
			vertices.push_back(topRight);
			vertices.push_back(bottomRight);
		}
	}
	vertexCount = static_cast<UINT>(vertices.size());

	D3D11_BUFFER_DESC description{};
	description.ByteWidth = static_cast<UINT>(vertices.size() * sizeof(Vertex));
	description.Usage = D3D11_USAGE_IMMUTABLE;
	description.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA initialData{};
	initialData.pSysMem = vertices.data();
	const HRESULT result =
		device->CreateBuffer(&description, &initialData, vertexBuffer.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(result), HRTrace(result));
}

void WaterRenderer::OnRender(const RenderContext& rc)
{
	if (!vertexBuffer || !constantBuffer) return;
	if (!vertexShader || !pixelShader) return;
	if (!rc.camera || !rc.deviceContext || !rc.renderState) return;

	const Transform* transform = owner->GetTransform();
	if (!transform) return;

	const Vector3 cameraPosition = rc.camera->GetEye();
	// 湖面はワールドへ固定する。カメラ追従させるとTerrainとの交差と頂点格子が
	// 毎フレームずれ、移動中に波が暴れて見える。
	const Matrix renderWorld = transform->matrix;

	ConstantBuffer constants{};
	constants.world = renderWorld;
	const Matrix view = rc.camera->GetView();
	const Matrix projection = rc.camera->GetProjection();
	constants.viewProjection = view * projection;
	constants.inverseViewProjection = constants.viewProjection.Invert();
	constants.cameraPosition = cameraPosition;
	constants.time = Game::Time::time;
	constants.shallowColor = settings.shallowColor;
	constants.deepColor = settings.deepColor;
	constants.waveScale = settings.waveScale;
	constants.waveSpeed = settings.waveSpeed;
	constants.waveStrength = settings.waveStrength;
	constants.fresnelPower = settings.fresnelPower;
	constants.fresnelStrength = settings.fresnelStrength;
	constants.opacity = std::clamp(settings.opacity, 0.0f, 1.0f);
	constants.shoreFadeDistance = std::max(settings.shoreFadeDistance, 0.01f);
	constants.screenSize = {Game::Graphics::ScreenWidth, Game::Graphics::ScreenHeight};
	constants.windDirection = {0.92f, 0.38f};
	constants.textureTiling = std::max(settings.waveScale, 0.01f);
	constants.normalIntensity = std::clamp(settings.waveStrength * 2.4f, 0.0f, 1.0f);
	constants.shininess = 96.0f;
	if (rc.lightManager)
	{
		const CbLightData lightData = rc.lightManager->ConvertToCb();
		constants.lightDirection = lightData.directionalLight.direction;
		constants.lightColor = lightData.directionalLight.color;
		constants.ambientColor = lightData.ambientColor;
	}
	else
	{
		constants.lightDirection = {0.0f, -1.0f, 0.0f};
		constants.lightColor = {1.0f, 1.0f, 1.0f, 1.0f};
		constants.ambientColor = {0.35f, 0.35f, 0.35f, 1.0f};
	}
	rc.deviceContext->UpdateSubresource(constantBuffer.Get(), 0, nullptr, &constants, 0, 0);

	const float blendFactor[4]{};
	rc.deviceContext->OMSetBlendState(
		rc.renderState->GetBlendState(BlendState::Transparency), blendFactor, 0xffffffff);
	rc.deviceContext->OMSetDepthStencilState(
		rc.renderState->GetDepthStencilState(DepthState::TestOnly), 0);
	rc.deviceContext->RSSetState(
		rc.renderState->GetRasterizerState(RasterizerState::SolidCullNone));
	rc.deviceContext->VSSetShader(vertexShader.Get(), nullptr, 0);
	rc.deviceContext->PSSetShader(pixelShader.Get(), nullptr, 0);
	rc.deviceContext->IASetInputLayout(inputLayout.Get());
	ID3D11Buffer* constantBuffers[] = {constantBuffer.Get()};
	rc.deviceContext->VSSetConstantBuffers(0, 1, constantBuffers);
	rc.deviceContext->PSSetConstantBuffers(0, 1, constantBuffers);
	RenderTarget* sceneBuffer =
		Game::Graphics::Instance().GetFrameBuffer(Game::FrameBufferId::Scene);
	if (sceneBuffer) sceneBuffer->SetDepthReadOnly(rc.deviceContext, true);
	ID3D11ShaderResourceView* waterTextures[] =
	{
		normalTexture ? normalTexture->GetShaderResourceView().Get() : nullptr,
		heightTexture ? heightTexture->GetShaderResourceView().Get() : nullptr,
		foamTexture ? foamTexture->GetShaderResourceView().Get() : nullptr,
		rc.iblData.specularPremappingRadianceEnvironmentMap,
		sceneBuffer ? sceneBuffer->GetDepthSRV() : nullptr,
	};
	rc.deviceContext->VSSetShaderResources(1, 1, &waterTextures[1]);
	rc.deviceContext->PSSetShaderResources(0, _countof(waterTextures), waterTextures);
	ID3D11SamplerState* sampler =
		rc.renderState->GetSamplerState(SamplerState::AnisotropicWrap);
	rc.deviceContext->VSSetSamplers(0, 1, &sampler);
	ID3D11SamplerState* pixelSamplers[] =
	{
		sampler,
		rc.renderState->GetSamplerState(SamplerState::LinearClamp),
	};
	rc.deviceContext->PSSetSamplers(0, _countof(pixelSamplers), pixelSamplers);
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	ID3D11Buffer* vertexBuffers[] = {vertexBuffer.Get()};
	rc.deviceContext->IASetVertexBuffers(0, 1, vertexBuffers, &stride, &offset);
	rc.deviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
	rc.deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	rc.deviceContext->Draw(vertexCount, 0);
	ID3D11ShaderResourceView* nullWaterTextures[_countof(waterTextures)]{};
	rc.deviceContext->VSSetShaderResources(1, 1, nullWaterTextures);
	rc.deviceContext->PSSetShaderResources(
		0, _countof(nullWaterTextures), nullWaterTextures);
	if (sceneBuffer) sceneBuffer->SetDepthReadOnly(rc.deviceContext, false);

	rc.deviceContext->OMSetBlendState(
		rc.renderState->GetBlendState(BlendState::Opaque), blendFactor, 0xffffffff);
	rc.deviceContext->OMSetDepthStencilState(
		rc.renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
	rc.deviceContext->RSSetState(
		rc.renderState->GetRasterizerState(RasterizerState::SolidCullBack));
}
