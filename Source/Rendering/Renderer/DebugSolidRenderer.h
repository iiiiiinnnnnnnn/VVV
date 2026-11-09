#pragma once
#include <memory>
#include <Dx11Kit/Dx11Kit.h>

#include <d3d11.h>
#include <wrl.h>

#include <vector>

#include "Core/Foundation/Common.h"

class DebugSolidRenderer
{
public:
	DebugSolidRenderer(ID3D11Device* device);

	// Blender風ボーンを追加
	void DrawBone(const Vector3& start, const Vector3& end, float width, const Color& color);

	// 追加済み形状を描画
	void Render(ID3D11DeviceContext* dc, const Matrix& view, const Matrix& projection);

private:
	struct Instance
	{
		Matrix transform;
		Color color;
	};

	struct CbMesh
	{
		Matrix worldViewProjection;
		Color color;
	};

	std::unique_ptr<Gpu::VertexBuffer> vertexBuffer;
	std::unique_ptr<Gpu::VertexShader> vertexShader;
	std::unique_ptr<Gpu::PixelShader> pixelShader;
	std::unique_ptr<Gpu::ConstantBuffer> constantBuffer;
	std::unique_ptr<Gpu::RasterizerState> rasterizerState;
	std::unique_ptr<Gpu::RasterizerState> outlineRasterizerState;
	std::unique_ptr<Gpu::BlendState> blendState;
	std::vector<Instance> instances;
	UINT vertexCount = 0;
};
