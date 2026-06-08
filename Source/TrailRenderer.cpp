// TrailRenderer.cpp

#include "Misc.h"
#include "GpuResourceUtils.h"
#include "TrailRenderer.h"

// コンストラクタ
TrailRenderer::TrailRenderer(ID3D11Device* device)
{
	D3D11_INPUT_ELEMENT_DESC inputElementDesc[]
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};

	// 頂点シェーダー
	GpuResourceUtils::LoadVertexShader(
		device,
		"Data/Shader/TrailRendererVS.cso",
		inputElementDesc,
		_countof(inputElementDesc),
		inputLayout.GetAddressOf(),
		vertexShader.GetAddressOf());

	// ピクセルシェーダー
	GpuResourceUtils::LoadPixelShader(
		device,
		"Data/Shader/TrailRendererPS.cso",
		pixelShader.GetAddressOf());

	// 定数バッファ
	GpuResourceUtils::CreateConstantBuffer(
		device,
		sizeof(CbScene),
		sceneConstant.GetAddressOf());

	// 頂点バッファ
	D3D11_BUFFER_DESC desc;
	desc.ByteWidth = sizeof(Vertex) * VertexCapacity;
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	desc.MiscFlags = 0;
	desc.StructureByteStride = 0;
	HRESULT hr = device->CreateBuffer(&desc, nullptr, vertexBuffer.GetAddressOf());
	_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
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
	const Matrix& projection)
{
	// シェーダー設定
	dc->VSSetShader(vertexShader.Get(), nullptr, 0);
	dc->PSSetShader(pixelShader.Get(), nullptr, 0);
	dc->IASetInputLayout(inputLayout.Get());

	// 定数バッファ設定
	ID3D11Buffer* constantBuffers[] =
	{
		sceneConstant.Get(),
	};
	dc->VSSetConstantBuffers(0, _countof(constantBuffers), constantBuffers);
	dc->PSSetConstantBuffers(0, _countof(constantBuffers), constantBuffers);

	// 定数バッファ更新
	CbScene cbScene;
	cbScene.viewProjection = view * projection;
	dc->UpdateSubresource(sceneConstant.Get(), 0, 0, &cbScene, 0, 0);

	// 頂点バッファ設定
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	dc->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT, 0);
	dc->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);

	// 描画
	UINT totalVertexCount = static_cast<UINT>(vertices.size());
	UINT start = 0;
	UINT count = (totalVertexCount < VertexCapacity) ? totalVertexCount : VertexCapacity;

	while (start < totalVertexCount)
	{
		D3D11_MAPPED_SUBRESOURCE mappedSubresource;
		HRESULT hr = dc->Map(vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubresource);
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

		memcpy(mappedSubresource.pData, &vertices[start], sizeof(Vertex) * count);

		dc->Unmap(vertexBuffer.Get(), 0);

		dc->Draw(count, 0);

		start += count;
		if ((start + count) > totalVertexCount)
		{
			count = totalVertexCount - start;
		}
	}

	vertices.clear();
}
