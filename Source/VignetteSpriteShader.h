// VignetteSpriteShader.h

#pragma once

#include "Shader.h"
#include "Graphics.h"

class VignetteSpriteShader : public SpriteShader
{
public:
	VignetteSpriteShader(ID3D11Device* device);
	~VignetteSpriteShader() {}

	void Begin(const RenderContext& rc) override;
	void Update(const RenderContext& rc, ID3D11ShaderResourceView* srv, Vector2 textureSize, const ShaderParamList& params) override;
	void End(const RenderContext& rc) override;

private:
	//	シェーダー用
	struct CbVignette
	{
		Vector4 color;
	};
	Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
};
