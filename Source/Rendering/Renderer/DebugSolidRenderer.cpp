#include "Rendering/Renderer/DebugSolidRenderer.h"

#include "Application/SettingsAndDebug/DebugUtil.h"
#include "Resource/GpuResourceUtils.h"

#include <array>
#include <cmath>

DebugSolidRenderer::DebugSolidRenderer(ID3D11Device* device)
{
	const D3D11_INPUT_ELEMENT_DESC element =
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0};
	GpuResourceUtils::LoadVertexShader(device, "Data/Shader/ShapeRendererVS.cso",
		&element, 1, inputLayout.GetAddressOf(), vertexShader.GetAddressOf());
	GpuResourceUtils::LoadPixelShader(device, "Data/Shader/ShapeRendererPS.cso", pixelShader.GetAddressOf());
	GpuResourceUtils::CreateConstantBuffer(device, sizeof(CbMesh), constantBuffer.GetAddressOf());

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
	_ASSERT_EXPR(SUCCEEDED(device->CreateBuffer(&bufferDesc, &data, vertexBuffer.GetAddressOf())), L"Debug bone buffer failed");

	D3D11_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.FillMode = D3D11_FILL_SOLID;
	rasterizerDesc.CullMode = D3D11_CULL_NONE;
	rasterizerDesc.DepthClipEnable = true;
	_ASSERT_EXPR(SUCCEEDED(device->CreateRasterizerState(&rasterizerDesc, rasterizerState.GetAddressOf())), L"Debug bone rasterizer failed");

	D3D11_RASTERIZER_DESC outlineRasterizerDesc = rasterizerDesc;
	outlineRasterizerDesc.FillMode = D3D11_FILL_WIREFRAME;
	outlineRasterizerDesc.AntialiasedLineEnable = true;
	_ASSERT_EXPR(SUCCEEDED(device->CreateRasterizerState(
		&outlineRasterizerDesc,
		outlineRasterizerState.GetAddressOf())), L"Debug bone outline rasterizer failed");

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
	_ASSERT_EXPR(SUCCEEDED(device->CreateBlendState(&blendDesc, blendState.GetAddressOf())), L"Debug bone blend state failed");
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
	Microsoft::WRL::ComPtr<ID3D11BlendState> previousBlendState;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> previousRasterizerState;
	FLOAT previousBlendFactor[4]{};
	UINT previousSampleMask = 0xFFFFFFFF;
	dc->OMGetBlendState(previousBlendState.GetAddressOf(), previousBlendFactor, &previousSampleMask);
	dc->RSGetState(previousRasterizerState.GetAddressOf());

	dc->VSSetShader(vertexShader.Get(), nullptr, 0);
	dc->PSSetShader(pixelShader.Get(), nullptr, 0);
	dc->IASetInputLayout(inputLayout.Get());
	dc->VSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());
	dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	dc->RSSetState(rasterizerState.Get());
	dc->OMSetBlendState(blendState.Get(), nullptr, 0xFFFFFFFF);

	const UINT stride = sizeof(Vector3);
	const UINT offset = 0;
	dc->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
	const auto renderInstances = [&](const Color* overrideColor)
	{
		for (const Instance& instance : instances)
		{
			CbMesh cb{};
			cb.worldViewProjection = instance.transform * view * projection;
			cb.color = overrideColor ? *overrideColor : instance.color;
			dc->UpdateSubresource(constantBuffer.Get(), 0, nullptr, &cb, 0, 0);
			dc->Draw(vertexCount, 0);
		}
	};

	// 半透明の面を描いたあと、同じ形状を黒いワイヤーで重ねて輪郭を明確にする
	dc->RSSetState(rasterizerState.Get());
	renderInstances(nullptr);
	dc->RSSetState(outlineRasterizerState.Get());
	const Color outlineColor(0.01f, 0.01f, 0.01f, 0.92f);
	renderInstances(&outlineColor);

	dc->OMSetBlendState(previousBlendState.Get(), previousBlendFactor, previousSampleMask);
	dc->RSSetState(previousRasterizerState.Get());
	instances.clear();
}
