#pragma once

#include "Shader.h"

class BasicSpriteShader : public SpriteShader
{
public:
	BasicSpriteShader(ID3D11Device* device);
	~BasicSpriteShader() {}

	void Begin(const RenderContext& rc) override;
	void Update(const RenderContext& rc, ID3D11ShaderResourceView* srv, Vector2 textureSize, Color color, float elapsedTime) override;
	void End(const RenderContext& rc) override;
};
