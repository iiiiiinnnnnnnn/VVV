#pragma once

#include "Gameplay/Actor/Actor.h"
#include "Rendering/Renderer/WaterRenderer.h"

// ワールドに配置される水面オブジェクト
class Water : public Actor
{
public:
	Water(const Transform& transform, const WaterRenderer::Settings& settings);

	void ApplySettings(
		bool enabled,
		const Transform& transform,
		const WaterRenderer::Settings& settings);

private:
	WaterRenderer* renderer = nullptr;
};
