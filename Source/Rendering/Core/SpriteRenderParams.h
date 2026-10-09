#pragma once
#include <memory>

class Texture;

struct SpriteDissolveParams
{
	std::shared_ptr<Texture> mask;
	float amount = 0.0f;
};

struct SpriteVignetteParams
{
	float range = 0.92f;
	float softness = 0.92f;
};

struct SpriteRenderParams
{
	SpriteVignetteParams vignette;
	SpriteDissolveParams dissolve;
};
