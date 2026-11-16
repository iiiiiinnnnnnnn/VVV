#pragma once
#include <memory>
#include <Dx11Kit/Dx11Kit.h>

class Texture;

struct SpriteDissolveParams
{
	std::shared_ptr<Texture> mask;
	float amount = 0.0f;
	float maskScale = 1.0f;
	float softness = 0.0f;
};

struct SpriteVignetteParams
{
	Vector2 center = {0.5f, 0.5f};
	float intensity = 1.29f;
	float smoothness = 0.7f;
	bool rounded = false;
	float roundness = 1.0f;
};

struct SpriteRenderParams
{
	SpriteVignetteParams vignette;
	SpriteDissolveParams dissolve;
};
