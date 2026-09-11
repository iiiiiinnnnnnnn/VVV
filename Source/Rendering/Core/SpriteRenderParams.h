#pragma once

struct SpriteVignetteParams
{
	float range = 0.92f;
	float softness = 0.92f;
};

struct SpriteRenderParams
{
	SpriteVignetteParams vignette;
};
