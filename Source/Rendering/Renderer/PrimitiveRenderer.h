#pragma once
#include <d3d11.h>
#include <wrl.h>

#include <vector>

#include "Core/Foundation/Common.h"

class RenderState;

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
	void DrawTriangle(const Vector3& a, const Vector3& b, const Vector3& c, const Color& color)
	{
		triangleVertices.push_back({a, color});
		triangleVertices.push_back({b, color});
		triangleVertices.push_back({c, color});
	}

	void RenderTriangles(
		ID3D11DeviceContext* dc,
		const Matrix& view,
		const Matrix& projection,
		RenderState* renderState);

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
	std::vector<Vertex>		triangleVertices;
	void RenderVertices(
		ID3D11DeviceContext* dc,
		const Matrix& view,
		const Matrix& projection,
		D3D11_PRIMITIVE_TOPOLOGY primitiveTopology,
		std::vector<Vertex>& sourceVertices);

	Microsoft::WRL::ComPtr<ID3D11VertexShader>	vertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>	pixelShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout>	inputLayout;
	Microsoft::WRL::ComPtr<ID3D11Buffer>		vertexBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer>		constantBuffer;
};
