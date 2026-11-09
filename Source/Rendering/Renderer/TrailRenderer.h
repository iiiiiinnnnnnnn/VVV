#pragma once
#include <memory>
#include <Dx11Kit/Dx11Kit.h>
#include <d3d11.h>
#include <wrl.h>

#include <vector>

#include "Core/Foundation/Common.h"

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
		Color color,
		Color endColor = Color(0.0f, 0.0f, 0.0f, -1.0f));

private:
	static const UINT VertexCapacity = 3 * 1024;

	struct CbTrail
	{
		Matrix		viewProjection;
		Color color;
		Color endColor;
	};
	std::unique_ptr<Gpu::ConstantBuffer> constant;

	struct Vertex
	{
		Vector3		position;
		Vector2 	uv;
	};
	std::vector<Vertex>		vertices;

	std::unique_ptr<Gpu::VertexShader> vertexShader;
	std::unique_ptr<Gpu::PixelShader> pixelShader;
	std::unique_ptr<Gpu::VertexBuffer> vertexBuffer;
};
