#pragma once
#include <d3d11.h>
#include <wrl.h>

#include <vector>

#include "Common.h"

class PrimitiveRenderer
{
public:
	PrimitiveRenderer(ID3D11Device* device);

	// 頂点追加
	void AddVertex(const Vector3& position, const Color& color);

	// 軸描画(D3D11_PRIMITIVE_TOPOLOGY_LINELIST)
	void DrawAxis(const Matrix& transform, const Color& color);

	// グリッド描画(D3D11_PRIMITIVE_TOPOLOGY_LINELIST)
	void DrawGrid(int subdivisions, float scale);

	void DrawLine(const Vector3& start, const Vector3& end, const Color& startColor, const Color& endColor)
	{
		AddVertex(start, startColor);
		AddVertex(end, endColor);
	}

	// 描画実行
	void Render(
		ID3D11DeviceContext* dc,
		const Matrix& view,
		const Matrix& projection,
		D3D11_PRIMITIVE_TOPOLOGY primitiveTopology);

private:
	static const UINT VertexCapacity = 3 * 1024;

	struct CbScene
	{
		Matrix		viewProjection;
	};

	struct Vertex
	{
		Vector3		position;
		Color		color;
	};
	std::vector<Vertex>		vertices;

	Microsoft::WRL::ComPtr<ID3D11VertexShader>	vertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>	pixelShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout>	inputLayout;
	Microsoft::WRL::ComPtr<ID3D11Buffer>		vertexBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer>		constantBuffer;
};
