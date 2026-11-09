#include <ProfileKit/ProfileKit.h>
#include <Dx11Kit/Dx11Kit.h>
#include "Rendering/Renderer/WaterRenderer.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "Application/Time/GameTime.h"
#include "Core/Object/Object.h"
#include "Gameplay/Camera/Camera.h"
#include "Gameplay/Lighting/LightManager.h"
#include "Rendering/Core/Graphics.h"
#include "Rendering/Core/RenderContext.h"
#include "Resource/ResourceManager.h"
#include "Resource/Texture.h"

WaterRenderer::WaterRenderer(Object* owner) : Component(owner)
{
}

void WaterRenderer::OnAwake()
{
	static_assert(sizeof(ConstantBuffer) % 16 == 0);

	ID3D11Device* device = Game::Graphics::Instance().GetDevice();
	if (!device) return;

	Gpu::InputLayoutDesc elements;
	elements.AddPosition()
		.AddTexcoord();
	vertexShader = std::make_unique<Gpu::VertexShader>(device, ResourceManager::Instance().ResolveShaderPath("Resources/Shader/WaterSurfaceVS.cso").c_str(),
		elements);
	pixelShader = std::make_unique<Gpu::PixelShader>(device, ResourceManager::Instance().ResolveShaderPath("Resources/Shader/WaterSurfacePS.cso").c_str());
	constantBuffer = std::make_unique<Gpu::ConstantBuffer>(device, sizeof(ConstantBuffer));
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

	vertexBuffer = std::make_unique<Gpu::VertexBuffer>(device, vertices.data(), static_cast<UINT>(vertices.size() * sizeof(Vertex)));
}

void WaterRenderer::ApplyUnderwaterContext(RenderContext& rc) const
{
	if (!IsActive() || !owner->IsActive() || owner->IsPendingDestroy() || !rc.camera) return;
	const Transform* transform = owner->GetTransform();
	if (!transform) return;
	if (std::abs(transform->GetMatrix().Determinant()) < 0.000001f) return;
	const Matrix inverseWorld = transform->GetMatrix().Invert();
	const Vector3 localEye = Vector3::Transform(rc.camera->GetEye(), inverseWorld);
	if (std::abs(localEye.x) > 0.5f || std::abs(localEye.z) > 0.5f || localEye.y >= 0.0f) return;
	// 水面直下の10cmで効果を滑らかに立ち上げる。波の細かな上下では切り替えない。
	const Vector3 surface = Vector3::Transform({localEye.x, 0.0f, localEye.z}, transform->GetMatrix());
	const float blend = std::clamp((surface - rc.camera->GetEye()).Length() / 0.1f, 0.0f, 1.0f);
	if (blend <= rc.underwaterBlend) return;
	rc.underwaterBlend = blend;
	rc.underwaterInverseWorld = inverseWorld;
}

void WaterRenderer::OnRender(const RenderContext& rc)
{
	PROFILE_FUNCTION();
	if (!vertexBuffer || !constantBuffer) return;
	if (!vertexShader || !pixelShader) return;
	if (!rc.camera || !rc.deviceContext || !rc.graphics) return;

	const Transform* transform = owner->GetTransform();
	if (!transform) return;

	const Vector3 cameraPosition = rc.camera->GetEye();
	// 湖面はワールドへ固定する。カメラ追従させるとTerrainとの交差と頂点格子が
	// 毎フレームずれ、移動中に波が暴れて見える。
	const Matrix renderWorld = transform->GetMatrix();

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
		constants.lightDirection = lightData.directionalLightCount ? lightData.directionalLights[0].direction : Vector3(0.0f, -1.0f, 0.0f);
		constants.lightColor = lightData.directionalLightCount ? lightData.directionalLights[0].color : Color(0.0f, 0.0f, 0.0f, 0.0f);
		constants.ambientColor = lightData.ambientColor;
	}
	else
	{
		constants.lightDirection = {0.0f, -1.0f, 0.0f};
		constants.lightColor = {1.0f, 1.0f, 1.0f, 1.0f};
		constants.ambientColor = {0.35f, 0.35f, 0.35f, 1.0f};
	}
	constantBuffer->Update(rc.deviceContext, &constants);

	const float blendFactor[4]{};
	rc.graphics->TransparentBlendState->Bind(rc.deviceContext, blendFactor);
	rc.graphics->DepthReadState->Bind(rc.deviceContext);
	rc.graphics->SolidRasterizerState->Bind(rc.deviceContext);
	vertexShader->Bind(rc.deviceContext);
	pixelShader->Bind(rc.deviceContext);
	constantBuffer->BindVS(rc.deviceContext, 0);
	constantBuffer->BindPS(rc.deviceContext, 0);
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
	Gpu::VertexShader::SetShaderResources(rc.deviceContext, 1, waterTextures[1]);
	Gpu::PixelShader::SetShaderResources(rc.deviceContext, 0, waterTextures);
	rc.graphics->AnisotropicWrapSamplerState->BindVS(rc.deviceContext, 0);
	Gpu::SamplerState::BindPS(rc.deviceContext, 0, {rc.graphics->AnisotropicWrapSamplerState.get(), rc.graphics->LinearClampSamplerState.get()});
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	vertexBuffer->Bind(rc.deviceContext, stride, 0, offset);
	Gpu::IndexBuffer::Unbind(rc.deviceContext, DXGI_FORMAT_UNKNOWN);
	Gpu::VertexBuffer::SetTopology(rc.deviceContext, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	vertexBuffer->Draw(rc.deviceContext, vertexCount);
	Gpu::VertexShader::UnbindShaderResources(rc.deviceContext, 1);
	Gpu::PixelShader::UnbindShaderResources(rc.deviceContext, 0, _countof(waterTextures));
	if (sceneBuffer) sceneBuffer->SetDepthReadOnly(rc.deviceContext, false);

	rc.graphics->OpaqueBlendState->Bind(rc.deviceContext, blendFactor);
	rc.graphics->DepthWriteState->Bind(rc.deviceContext);
	rc.graphics->BackCullRasterizerState->Bind(rc.deviceContext);
}
