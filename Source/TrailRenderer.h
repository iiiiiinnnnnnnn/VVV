#pragma once

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
		const Matrix& projection);

private:
	static const UINT VertexCapacity = 3 * 1024;

	struct CbScene
	{
		Matrix		viewProjection;
	};
	Microsoft::WRL::ComPtr<ID3D11Buffer> sceneConstant;

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
