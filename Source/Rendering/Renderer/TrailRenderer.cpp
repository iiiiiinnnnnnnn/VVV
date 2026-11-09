#include "Resource/ResourceManager.h"
#include <ProfileKit/ProfileKit.h>
#include <Dx11Kit/Dx11Kit.h>
#include "Application/SettingsAndDebug/DebugUtil.h"
#include "Rendering/Renderer/TrailRenderer.h"

// コンストラクタ
TrailRenderer::TrailRenderer(ID3D11Device* device)
{
	Gpu::InputLayoutDesc inputElementDesc;
	inputElementDesc.AddPosition()
		.AddTexcoord();

	// 頂点シェーダー
	vertexShader = std::make_unique<Gpu::VertexShader>(device, ResourceManager::Instance().ResolveShaderPath("Resources/Shader/TrailRendererVS.cso").c_str(),
		inputElementDesc);

	// ピクセルシェーダー
	pixelShader = std::make_unique<Gpu::PixelShader>(device, ResourceManager::Instance().ResolveShaderPath("Resources/Shader/TrailRendererPS.cso").c_str());

	// 定数バッファ
	constant = std::make_unique<Gpu::ConstantBuffer>(device, sizeof(CbTrail));

	// 頂点バッファ
	const Gpu::BufferDesc desc(sizeof(Vertex) * VertexCapacity,
		D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
	vertexBuffer = std::make_unique<Gpu::VertexBuffer>(device, desc, nullptr);
}

void TrailRenderer::AddPoint(const Vector3& root, const Vector3& tip, float uvY)
{
	Vertex& v1 = vertices.emplace_back();
	v1.position = root;
	v1.uv = { 0.0f, uvY };

	Vertex& v2 = vertices.emplace_back();
	v2.position = tip;
	v2.uv = { 1.0f, uvY };
}

// 描画実行
void TrailRenderer::Render(
	ID3D11DeviceContext* dc,
	const Matrix& view,
	const Matrix& projection,
	Color color,
	Color endColor)
{
	PROFILE_FUNCTION();
	// シェーダー設定
	vertexShader->Bind(dc);
	pixelShader->Bind(dc);

	// 定数バッファ設定
	constant->BindVS(dc, 0);
	constant->BindPS(dc, 0);

	// 定数バッファ更新
	CbTrail cbTrail;
	cbTrail.viewProjection = view * projection;
	cbTrail.color = color;
	if (endColor.w < 0.0f) endColor = Color(color.x, color.y, color.z, 0.0f);
	cbTrail.endColor = endColor;
	constant->Update(dc, &cbTrail);

	// 頂点バッファ設定
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	Gpu::VertexBuffer::SetTopology(dc, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	Gpu::IndexBuffer::Unbind(dc, DXGI_FORMAT_R32_UINT);
	vertexBuffer->Bind(dc, stride, 0, offset);

	// 描画
	UINT totalVertexCount = static_cast<UINT>(vertices.size());
	UINT start = 0;
	UINT count = (totalVertexCount < VertexCapacity) ? totalVertexCount : VertexCapacity;

	while (start < totalVertexCount)
	{
		vertexBuffer->Update(dc, &vertices[start], static_cast<UINT>(sizeof(Vertex) * count));

		vertexBuffer->Draw(dc, count);

		start += count;
		if ((start + count) > totalVertexCount)
		{
			count = totalVertexCount - start;
		}
	}

	vertices.clear();
}
