#pragma once

#include <vector>

// Global, unscaled-time driven controller for short slow-motion and freeze
// effects. Multiple requests may overlap; the strongest slowdown wins.
class TimeScaleController
{
public:
	// scale is a multiplier in [0, 1]. A value of 0 creates a full hit stop.
	static void Request(float duration, float scale = 0.0f);
	static void Update();
	static void CancelAll();

	static bool IsPlaying() { return !effects.empty(); }
	static float GetAppliedScale() { return appliedScale; }

private:
	struct Effect
	{
		float remaining = 0.0f;
		float scale = 1.0f;
	};

	static void ApplyStrongestEffect();

	inline static std::vector<Effect> effects;
	inline static float baseScale = 1.0f;
	inline static float appliedScale = 1.0f;
};
