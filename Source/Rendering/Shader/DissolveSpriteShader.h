#pragma once
#include <memory>
#include "Rendering/Core/ConstantBuffer.h"

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
		float maskScale;
		float softness;
		float padding;
	};
	std::unique_ptr<Gpu::ConstantBuffer> constantBuffer;
};
