// TrailRenderer.h

#pragma once
#include <d3d11.h>
#include <wrl.h>

#include <vector>

#include "Common.h"

class TrailRenderer
{
public:
	TrailRenderer(ID3D11Device* device);

	void AddPoint(const Vector3& root, const Vector3& tip, float uvY = 0.0f);

	// 描画実行
	void Render(
		ID3D11DeviceContext* dc,
		const Matrix& view,
		const Matrix& projection,
		Color color);

private:
	static const UINT VertexCapacity = 3 * 1024;

	struct CbTrail
	{
		Matrix		viewProjection;
		Color color;
	};
	Microsoft::WRL::ComPtr<ID3D11Buffer> constant;

	struct Vertex
	{
		Vector3		position;
		Vector2 	uv;
	};
	std::vector<Vertex>		vertices;

	Microsoft::WRL::ComPtr<ID3D11VertexShader>	vertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>	pixelShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout>	inputLayout;
	Microsoft::WRL::ComPtr<ID3D11Buffer>		vertexBuffer;
};
