// ThreatenLineSpriteShader.h

#pragma once
#include <d3d11.h>
#include <wrl.h>

#include "Shader.h"
#include "Graphics.h"

class ThreatenLineSpriteShader : public SpriteShader
{
public:
	ThreatenLineSpriteShader(ID3D11Device* device);
	~ThreatenLineSpriteShader() {}

	void Begin(const RenderContext& rc) override;
	void Update(const RenderContext& rc, ID3D11ShaderResourceView* srv, Vector2 textureSize, const ShaderParamList& params) override;
	void End(const RenderContext& rc) override;

private:
	//	シェーダー用
    struct CbThreatenLine
    {
        Color color = Color(1, 1, 1, 0.35f);

        Vector2 center = Vector2(0.5f, 0.5f);
        float screenAspect = 1280.0f / 720.0f;
        float lineCount = 64.0f;

        float lineWidth = 0.14f;
        float softness = 0.08f;
        float innerRadius = 0.18f;
        float outerRadius = 0.98f;

        float randomStrength = 0.45f;
        float randomSeed = 0.0f;
        float rotation = 0.0f;
        float alphaMultiplier = 1.0f;

        float time = 0.0f;
        float randomChangeSpeed = 12.0f;
        float rotationSpeed = 0.0f;
        float noiseScroll = 0.0f;
    };

	Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
};
