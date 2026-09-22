#pragma once

#include "Rendering/Shader/Shader.h"

// UV中心から外周へ暗色を重ねる、旧スプライト用Vignetteシェーダー。
class VignetteSpriteShader : public SpriteShader
{
public:
	VignetteSpriteShader(ID3D11Device* device, bool overlayOnly = false);

	void Begin(const RenderContext& rc) override;
	void Update(const RenderContext& rc, ID3D11ShaderResourceView* srv,
		Vector2 textureSize, const Color& color,
		const SpriteRenderParams* params) override;
	void End(const RenderContext& rc) override;

private:
	struct CbVignette
	{
		Color color;
		Vector2 textureSize;
		float range;
		float softness;
		float overlayOnly;
		float padding[3];
	};

	Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
	bool overlayOnly = false;
};
