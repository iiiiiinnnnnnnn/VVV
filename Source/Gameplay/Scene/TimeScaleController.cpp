#include "Gameplay/Scene/TimeScaleController.h"

#include <algorithm>

#include "Application/Time/GameTime.h"

void TimeScaleController::Request(float duration, float scale)
{
	if (duration <= 0.0f) return;

	if (effects.empty()) baseScale = Game::Time::scale;
	effects.push_back({duration, std::clamp(scale, 0.0f, 1.0f)});
	ApplyStrongestEffect();
}

void TimeScaleController::Update()
{
	if (effects.empty()) return;

	for (Effect& effect : effects)
		effect.remaining -= Game::Time::unscaledDeltaTime;

	std::erase_if(effects, [](const Effect& effect) { return effect.remaining <= 0.0f; });
	if (effects.empty())
	{
		Game::Time::scale = baseScale;
		appliedScale = 1.0f;
		return;
	}

	ApplyStrongestEffect();
}

void TimeScaleController::CancelAll()
{
	if (effects.empty()) return;

	effects.clear();
	Game::Time::scale = baseScale;
	appliedScale = 1.0f;
}

void TimeScaleController::ApplyStrongestEffect()
{
	appliedScale = 1.0f;
	for (const Effect& effect : effects)
		appliedScale = std::min(appliedScale, effect.scale);

	Game::Time::scale = baseScale * appliedScale;
}
