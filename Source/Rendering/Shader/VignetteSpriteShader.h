#pragma once
#include <memory>
#include <Dx11Kit/Dx11Kit.h>

#include "Rendering/Shader/Shader.h"

// UV中心から外周へ暗色を重ねる、スプライト用Vignetteシェーダー。
class VignetteSpriteShader : public SpriteShader
{
public:
	VignetteSpriteShader(ID3D11Device* device);

	void Begin(const RenderContext& rc) override;
	void Update(const RenderContext& rc, ID3D11ShaderResourceView* srv,
		Vector2 textureSize, const Color& color,
		const SpriteRenderParams* params) override;
	void End(const RenderContext& rc) override;

private:
	struct CbVignette
	{
		Color color;
		Vector2 center;
		float intensity;
		float smoothness;
		float rounded;
		float roundness;
		float padding[2];
		Color tintColor;
		float tintIntensity;
		float spriteMode;
		float padding2[2];
	};

	std::unique_ptr<Gpu::ConstantBuffer> constantBuffer;
};
