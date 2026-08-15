#pragma once

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

	Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
	Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
	Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerState;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> outlineRasterizerState;
	Microsoft::WRL::ComPtr<ID3D11BlendState> blendState;
	std::vector<Instance> instances;
	UINT vertexCount = 0;
};
