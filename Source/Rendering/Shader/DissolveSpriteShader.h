#pragma once

#include "Rendering/Shader/Shader.h"

class DissolveSpriteShader : public SpriteShader
{
public:
	DissolveSpriteShader(ID3D11Device* device);
	void Begin(const RenderContext& rc) override;
	void Update(const RenderContext& rc, ID3D11ShaderResourceView* srv,
		Vector2 textureSize, const Color& color, const SpriteRenderParams* params) override;
	void End(const RenderContext& rc) override;

private:
	struct CbDissolve
	{
		Color color;
		float amount;
		float padding[3];
	};
	Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
};
