#include "Resource/ResourceManager.h"
#include "Rendering/Renderer/DebugSolidRenderer.h"
#include <Dx11Kit/Dx11Kit.h>

#include "Application/SettingsAndDebug/DebugUtil.h"

#include <array>
#include <cmath>
#include <stdexcept>

DebugSolidRenderer::DebugSolidRenderer(ID3D11Device* device)
{
	const D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
	vertexShader = std::make_unique<Gpu::VertexShader>(device, ResourceManager::Instance().ResolveShaderPath("Resources/Shader/ShapeRendererVS.cso").c_str(),
		layout);
	pixelShader = std::make_unique<Gpu::PixelShader>(device, ResourceManager::Instance().ResolveShaderPath("Resources/Shader/ShapeRendererPS.cso").c_str());
	constantBuffer = std::make_unique<Gpu::ConstantBuffer>(device, sizeof(CbMesh));

	// Root、太いJoint、Tipで八面体を作る
	const Vector3 root(0, 0, 0);
	const Vector3 tip(0, 0, 1);
	const Vector3 right(1, 0, 0.2f);
	const Vector3 left(-1, 0, 0.2f);
	const Vector3 top(0, 1, 0.2f);
	const Vector3 bottom(0, -1, 0.2f);
	const std::array<Vector3, 24> vertices = {
		root, right, top, root, top, left,
		root, left, bottom, root, bottom, right,
		tip, top, right, tip, left, top,
		tip, bottom, left, tip, right, bottom};
	vertexCount = static_cast<UINT>(vertices.size());

	D3D11_BUFFER_DESC bufferDesc{};
	bufferDesc.ByteWidth = sizeof(vertices);
	bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA data{};
	data.pSysMem = vertices.data();
	vertexBuffer = std::make_unique<Gpu::VertexBuffer>(device, bufferDesc, &data);

	D3D11_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.FillMode = D3D11_FILL_SOLID;
	rasterizerDesc.CullMode = D3D11_CULL_NONE;
	rasterizerDesc.DepthClipEnable = true;
	rasterizerState = std::make_unique<Gpu::RasterizerState>(device, rasterizerDesc);

	D3D11_RASTERIZER_DESC outlineRasterizerDesc = rasterizerDesc;
	outlineRasterizerDesc.FillMode = D3D11_FILL_WIREFRAME;
	outlineRasterizerDesc.AntialiasedLineEnable = true;
	outlineRasterizerState = std::make_unique<Gpu::RasterizerState>(device, outlineRasterizerDesc);

	D3D11_BLEND_DESC blendDesc{};
	D3D11_RENDER_TARGET_BLEND_DESC& target = blendDesc.RenderTarget[0];
	target.BlendEnable = true;
	target.SrcBlend = D3D11_BLEND_SRC_ALPHA;
	target.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	target.BlendOp = D3D11_BLEND_OP_ADD;
	target.SrcBlendAlpha = D3D11_BLEND_ONE;
	target.DestBlendAlpha = D3D11_BLEND_ZERO;
	target.BlendOpAlpha = D3D11_BLEND_OP_ADD;
	target.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	blendState = std::make_unique<Gpu::BlendState>(device, blendDesc);
}

void DebugSolidRenderer::DrawBone(const Vector3& start, const Vector3& end, float width, const Color& color)
{
	Vector3 forward = end - start;
	const float length = forward.Length();
	if (length <= 0.00001f) return;
	forward /= length;

	Vector3 reference = std::abs(forward.Dot(Vector3::Up)) > 0.98f ? Vector3::UnitX : Vector3::Up;
	Vector3 right = reference.Cross(forward);
	right.Normalize();
	const Vector3 up = forward.Cross(right);

	Instance& instance = instances.emplace_back();
	instance.transform = Matrix(
		right.x * width, right.y * width, right.z * width, 0,
		up.x * width, up.y * width, up.z * width, 0,
		forward.x * length, forward.y * length, forward.z * length, 0,
		start.x, start.y, start.z, 1);
	instance.color = color;
}

void DebugSolidRenderer::Render(ID3D11DeviceContext* dc, const Matrix& view, const Matrix& projection)
{
	if (instances.empty()) return;
	const auto previousBlendState = Gpu::BlendState::Capture(dc);
	const auto previousRasterizerState = Gpu::RasterizerState::Capture(dc);

	vertexShader->Bind(dc);
	pixelShader->Bind(dc);
	constantBuffer->BindVS(dc, 0);
	Gpu::VertexBuffer::SetTopology(dc, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	rasterizerState->Bind(dc);
	blendState->Bind(dc);

	const UINT stride = sizeof(Vector3);
	const UINT offset = 0;
	vertexBuffer->Bind(dc, stride, 0, offset);
	const auto renderInstances = [&](const Color* overrideColor)
	{
		for (const Instance& instance : instances)
		{
			CbMesh cb{};
			cb.worldViewProjection = instance.transform * view * projection;
			cb.color = overrideColor ? *overrideColor : instance.color;
			constantBuffer->Update(dc, &cb);
			vertexBuffer->Draw(dc, vertexCount);
		}
	};

	// 半透明の面を描いたあと、同じ形状を黒いワイヤーで重ねて輪郭を明確にする
	rasterizerState->Bind(dc);
	renderInstances(nullptr);
	outlineRasterizerState->Bind(dc);
	const Color outlineColor(0.01f, 0.01f, 0.01f, 0.92f);
	renderInstances(&outlineColor);

	Gpu::BlendState::Restore(dc, previousBlendState);
	Gpu::RasterizerState::Restore(dc, previousRasterizerState.Get());
	instances.clear();
}
