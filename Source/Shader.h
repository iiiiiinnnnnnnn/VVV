#pragma once

#include "Common.h"
#include "RenderContext.h"
#include "Model.h"

class SpriteShader
{
public:
	SpriteShader() {}
	virtual ~SpriteShader() {}

	virtual void Begin(const RenderContext& rc) = 0;
	virtual void Update(const RenderContext& rc, ID3D11ShaderResourceView* srv, float r, float g, float b, float a, float elapsedTime) = 0;
	virtual void ApplyParams(std::shared_ptr<void> params) {}
	virtual void End(const RenderContext& rc) = 0;

protected:
	Microsoft::WRL::ComPtr<ID3D11VertexShader>		vertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>		pixelShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout>		inputLayout;

	static const std::vector<D3D11_INPUT_ELEMENT_DESC> InputElementDescs;
};

class ModelShader
{
public:
	ModelShader() {}
	virtual ~ModelShader() {}

	virtual void Begin(const RenderContext& rc) = 0;
	virtual void Update(const RenderContext& rc, const Model::Mesh& mesh, float elapsedTime) = 0;
	virtual void End(const RenderContext& rc) = 0;

protected:
	Microsoft::WRL::ComPtr<ID3D11VertexShader>		vertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>		pixelShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout>		inputLayout;

	static const std::vector<D3D11_INPUT_ELEMENT_DESC> InputElementDescs;
};
