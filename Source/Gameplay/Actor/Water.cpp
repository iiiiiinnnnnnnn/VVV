#include "Gameplay/Actor/Water.h"
#include <ImGuiToolKit/Utf8.h>

Water::Water(const Transform& transform, const WaterRenderer::Settings& settings)
	: Actor(JText(u8"ワールド水面"), "WorldWater", true, transform), settings(settings)
{}

Water::~Water()
{
	Uninit();
}

void Water::OnAwake()
{
	renderer = AddComponent<WaterRenderer>();
	renderer->SetSettings(settings);
}

void Water::ApplySettings(
	bool enabled,
	const Transform& transform,
	const WaterRenderer::Settings& settings)
{
	this->transform = transform;
	SetActive(enabled);
	this->settings = settings;

	if (renderer)
	{
		renderer->SetSettings(settings);
	}
}
