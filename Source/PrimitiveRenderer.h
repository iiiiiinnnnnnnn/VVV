#pragma once

#include "Common.h"

class PrimitiveRenderer
{
public:
	PrimitiveRenderer(ID3D11Device* device);

	// í∏ì_í«â¡
	void AddVertex(const Vector3& position, const Color& color);

	// é≤ï`âÊ(D3D11_PRIMITIVE_TOPOLOGY_LINELIST)
	void DrawAxis(const Matrix& transform, const Color& color);

	// ÉOÉäÉbÉhï`âÊ(D3D11_PRIMITIVE_TOPOLOGY_LINELIST)
	void DrawGrid(int subdivisions, float scale);

	// ï`âÊé¿çs
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
		Color		color;
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
