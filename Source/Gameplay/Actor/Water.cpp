#include "Gameplay/Actor/Water.h"

Water::Water(const Transform& transform, const WaterRenderer::Settings& settings)
	: Actor((const char*)u8"ワールド水面", "WorldWater", true)
{
	this->transform = transform;
	this->transform.Update();
	renderer = AddComponent<WaterRenderer>();
	renderer->SetSettings(settings);
}

void Water::ApplySettings(
	bool enabled,
	const Transform& transform,
	const WaterRenderer::Settings& settings)
{
	this->transform = transform;
	this->transform.Update();
	SetActive(enabled);

	if (renderer)
	{
		renderer->SetSettings(settings);
	}
}
