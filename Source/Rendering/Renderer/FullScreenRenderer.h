#pragma once
#include <Dx11Kit/Dx11Kit.h>
#include <DirectXMath.h>
#include <memory>

// 全画面描画に共通する頂点、入力レイアウト、VertexShaderを所有
// PixelShader、定数バッファ、描画ステートは呼び出し側で設定する想定で
class FullScreenRenderer
{
public:
	explicit FullScreenRenderer(ID3D11Device* device);
	void Draw(ID3D11DeviceContext* context) const;

private:
	struct Vertex
	{
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT2 texcoord;
	};
	static constexpr UINT VertexCount = 4;
	std::unique_ptr<Gpu::VertexBuffer> vertexBuffer;
	std::unique_ptr<Gpu::VertexShader> vertexShader;
};
