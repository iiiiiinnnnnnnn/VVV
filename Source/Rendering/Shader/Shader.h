// Shader.h
#pragma once
#include <d3d11.h>
#include <wrl.h>

#include <vector>

#include "Core/Foundation/Common.h"
#include "Rendering/Core/RenderContext.h"
#include "Rendering/Core/VMatRenderParams.h"
#include "Resource/VMDLModel.h"

class Shader
{
public:
	Shader() = default;
	virtual ~Shader() = default;
	virtual void Begin(const RenderContext& rc) = 0;
	virtual void End(const RenderContext& rc) = 0;

protected:
	Microsoft::WRL::ComPtr<ID3D11VertexShader>		vertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>		pixelShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout>		inputLayout;
};

class SpriteShader : public Shader
{
public:
	virtual void Update(
		const RenderContext& rc,
		ID3D11ShaderResourceView* srv,
		Vector2 textureSize,
		const Color& color) = 0;
	static const std::vector<D3D11_INPUT_ELEMENT_DESC> InputElementDescs;
};

class ModelShader : public Shader
{
public:
	virtual void Update(
		const RenderContext& rc,
		const VMDLModel::Mesh& mesh,
		const VMatRenderParams* params) = 0;
	static const std::vector<D3D11_INPUT_ELEMENT_DESC> InputElementDescs;
};
