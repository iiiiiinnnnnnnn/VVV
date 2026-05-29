#pragma once

#include "Common.h"
#include "RenderContext.h"
#include "Model.h"

using ShaderParamPtr = const void*;

#define SHADER_PARAMS(...)                                      \
struct Params { __VA_ARGS__ };                                  \
void ApplyParams(ShaderParamPtr p) override {					\
    if (p)														\
		params = *static_cast<const Params*>(p);                \
    else 														\
		params = Params();										\
}                                                               \
Params params

class Shader
{
public:
	Shader() = default;
	virtual ~Shader() = default;
	virtual void Begin(const RenderContext& rc) = 0;
	virtual void End(const RenderContext& rc) = 0;
	virtual void ApplyParams(ShaderParamPtr params) {}

protected:
	Microsoft::WRL::ComPtr<ID3D11VertexShader>		vertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>		pixelShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout>		inputLayout;
};

class SpriteShader : public Shader
{
public:
	virtual void Update(const RenderContext& rc, ID3D11ShaderResourceView* srv, Vector2 textureSize, Color color, float elapsedTime) = 0;

	static const std::vector<D3D11_INPUT_ELEMENT_DESC> InputElementDescs;
};

class ModelShader : public Shader
{
public:
	virtual void Update(const RenderContext& rc, const Model::Mesh& mesh, float elapsedTime) = 0;

	static const std::vector<D3D11_INPUT_ELEMENT_DESC> InputElementDescs;
};
