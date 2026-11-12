#include "Resource/ResourceManager.h"
#include "Application/SettingsAndDebug/DebugUtil.h"
#include <Dx11Kit/Dx11Kit.h>
#include "Rendering/Renderer/PrimitiveRenderer.h"
#include "Rendering/Core/Graphics.h"

// コンストラクタ
PrimitiveRenderer::PrimitiveRenderer(ID3D11Device* device)
{
	const D3D11_INPUT_ELEMENT_DESC inputElementDesc[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
	// 頂点シェーダー
	vertexShader = std::make_unique<Gpu::VertexShader>(device, ResourceManager::Instance().ResolveShaderPath("Resources/Shader/PrimitiveRendererVS.cso").c_str(),
		inputElementDesc);

	// ピクセルシェーダー
	pixelShader = std::make_unique<Gpu::PixelShader>(device, ResourceManager::Instance().ResolveShaderPath("Resources/Shader/PrimitiveRendererPS.cso").c_str());

	// 定数バッファ
	constantBuffer = std::make_unique<Gpu::ConstantBuffer>(device, sizeof(CbScene));

	// 頂点バッファ
	D3D11_BUFFER_DESC desc{};
	desc.ByteWidth = sizeof(Vertex) * VertexCapacity;
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	vertexBuffer = std::make_unique<Gpu::VertexBuffer>(device, desc, nullptr);
}

// 頂点追加
void PrimitiveRenderer::AddVertex(const Vector3& position, const Color& color)
{
	Vertex& v = vertices.emplace_back();
	v.position = position;
	v.color = color;
}

// 軸描画
void PrimitiveRenderer::DrawAxis(const Matrix& transform, const Color& color)
{
	DirectX::XMMATRIX W = transform;
	Vector3 p, x, y, z;
	p = W.r[3];
	x = DirectX::XMVector3Transform(Vector3(1, 0, 0), W);
	y = DirectX::XMVector3Transform(Vector3(0, 1, 0), W);
	z = DirectX::XMVector3Transform(Vector3(0, 0, 1), W);
	AddVertex(p, { 1, 0, 0, 1 });
	AddVertex(x, { 1, 0, 0, 1 });
	AddVertex(p, { 0, 1, 0, 1 });
	AddVertex(y, { 0, 1, 0, 1 });
	AddVertex(p, { 0, 0, 1, 1 });
	AddVertex(z, { 0, 0, 1, 1 });
}

// グリッド描画
void PrimitiveRenderer::DrawGrid(int subdivisions, float scale)
{
	int numLines = (subdivisions + 1) * 2;
	int vertexCount = numLines * 2;

	float corner = 0.5f;
	float step = 1.0f / static_cast<float>(subdivisions);

	int index = 0;
	float s = -corner;

	const Color white = Color(1, 1, 1, 1);

	// Create vertical lines
	float scaling = static_cast<float>(subdivisions) * scale;
	DirectX::XMMATRIX M = Matrix::CreateScale(scaling);
	DirectX::XMVECTOR V, P;
	Vector3 position;
	for (int i = 0; i <= subdivisions; i++)
	{
		V = Vector3(s, 0, corner);
		P = DirectX::XMVector3TransformCoord(V, M);
		position = P;
		AddVertex(position, white);

		V = Vector3(s, 0, -corner);
		P = DirectX::XMVector3TransformCoord(V, M);
		position = P;
		AddVertex(position, white);

		s += step;
	}

	// Create horizontal lines
	s = -corner;
	for (int i = 0; i <= subdivisions; i++)
	{
		V = Vector3(corner, 0, s);
		P = DirectX::XMVector3TransformCoord(V, M);
		position = P;
		AddVertex(position, white);

		V = Vector3(-corner, 0, s);
		P = DirectX::XMVector3TransformCoord(V, M);
		position = P;
		AddVertex(position, white);

		s += step;
	}

	// X軸
	{
		const Color red = Color(1, 0, 0, 1);
		V = Vector3(0, 0, 0);
		P = DirectX::XMVector3TransformCoord(V, M);
		position = P;
		AddVertex(position, red);

		V = Vector3(corner, 0, 0);
		P = DirectX::XMVector3TransformCoord(V, M);
		position = P;
		AddVertex(position, red);
	}

	// Y軸
	{
		const Color green = Color(0, 1, 0, 1);
		V = Vector3(0, 0, 0);
		P = DirectX::XMVector3TransformCoord(V, M);
		position = P;
		AddVertex(position, green);

		V = Vector3(0, corner, 0);
		P = DirectX::XMVector3TransformCoord(V, M);
		position = P;
		AddVertex(position, green);
	}

	// Z軸
	{
		const Color blue = Color(0, 0, 1, 1);
		V = Vector3(0, 0, 0);
		P = DirectX::XMVector3TransformCoord(V, M);
		position = P;
		AddVertex(position, blue);

		V = Vector3(0, 0, corner);
		P = DirectX::XMVector3TransformCoord(V, M);
		position = P;
		AddVertex(position, blue);
	}
}

// 描画実行
void PrimitiveRenderer::Render(
	ID3D11DeviceContext* dc,
	const Matrix& view,
	const Matrix& projection,
	D3D11_PRIMITIVE_TOPOLOGY primitiveTopology)

{
	RenderVertices(dc, view, projection, primitiveTopology, vertices);
}

void PrimitiveRenderer::RenderTriangles(
	ID3D11DeviceContext* dc,
	const Matrix& view,
	const Matrix& projection,
	const Game::Graphics* graphicsResources)
{
	if (triangleVertices.empty()) return;
	const auto previousBlend = Gpu::BlendState::Capture(dc);
	const auto previousDepth = Gpu::DepthStencilState::Capture(dc);
	const auto previousRasterizer = Gpu::RasterizerState::Capture(dc);
	graphicsResources->TransparentBlendState->Bind(dc);
	graphicsResources->DepthReadState->Bind(dc);
	graphicsResources->SolidRasterizerState->Bind(dc);
	RenderVertices(dc, view, projection, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, triangleVertices);
	Gpu::BlendState::Restore(dc, previousBlend);
	Gpu::DepthStencilState::Restore(dc, previousDepth);
	Gpu::RasterizerState::Restore(dc, previousRasterizer.Get());
}

void PrimitiveRenderer::RenderVertices(
	ID3D11DeviceContext* dc,
	const Matrix& view,
	const Matrix& projection,
	D3D11_PRIMITIVE_TOPOLOGY primitiveTopology,
	std::vector<Vertex>& sourceVertices)
{
	// シェーダー設定
	vertexShader->Bind(dc);
	pixelShader->Bind(dc);

	// 定数バッファ設定
	constantBuffer->BindVS(dc, 0);

	// ビュープロジェクション行列作成
	DirectX::XMMATRIX V = DirectX::XMLoadFloat4x4(&view);
	DirectX::XMMATRIX P = DirectX::XMLoadFloat4x4(&projection);
	DirectX::XMMATRIX VP = V * P;

	// 定数バッファ更新
	CbScene cbScene;
	DirectX::XMStoreFloat4x4(&cbScene.viewProjection, VP);
	constantBuffer->Update(dc, &cbScene);

	// 頂点バッファ設定
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	Gpu::VertexBuffer::SetTopology(dc, primitiveTopology);
	Gpu::IndexBuffer::Unbind(dc, DXGI_FORMAT_R32_UINT);
	vertexBuffer->Bind(dc, stride, 0, offset);

	// 描画
	UINT totalVertexCount = static_cast<UINT>(sourceVertices.size());
	UINT start = 0;
	UINT count = (totalVertexCount < VertexCapacity) ? totalVertexCount : VertexCapacity;

	while (start < totalVertexCount)
	{
		vertexBuffer->Update(dc, &sourceVertices[start], static_cast<UINT>(sizeof(Vertex) * count));

		vertexBuffer->Draw(dc, count);

		start += count;
		if ((start + count) > totalVertexCount)
		{
			count = totalVertexCount - start;
		}
	}

	sourceVertices.clear();
}
