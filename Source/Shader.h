#pragma once

#include "Common.h"
#include "RenderContext.h"
#include "Model.h"
#include "ShaderParam.h"

class Shader
{
public:
	Shader() = default;
	virtual ~Shader() = default;
	virtual void Begin(const RenderContext& rc) = 0;
	virtual void End(const RenderContext& rc) = 0;
	virtual void ApplyShaderParams(const ShaderParamList& params) { cachedParams = params; }

protected:
	Microsoft::WRL::ComPtr<ID3D11VertexShader>		vertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>		pixelShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout>		inputLayout;

	ShaderParamList cachedParams;
};

class SpriteShader : public Shader
{
public:
	virtual void Update(const RenderContext& rc, ID3D11ShaderResourceView* srv, Vector2 textureSize, const ShaderParamList& params, float elapsedTime) = 0;
	static const std::vector<D3D11_INPUT_ELEMENT_DESC> InputElementDescs;
};

class ModelShader : public Shader
{
public:
	virtual void Update(const RenderContext& rc, const Model::Mesh& mesh, float elapsedTime) = 0;
	static const std::vector<D3D11_INPUT_ELEMENT_DESC> InputElementDescs;
};
