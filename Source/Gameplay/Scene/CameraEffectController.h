#pragma once

#include "Core/Foundation/Common.h"
#include "Gameplay/Camera/Camera.h"
#include "Application/Time/GameTime.h"

class CameraEffectController
{
public:
    static void Request(float duration, float intensity);
	static void RequestFovOffset(float duration, float offsetDegrees)
	{
		if (duration <= 0.0f) return;
		fovTimer = std::max(fovTimer, duration);
		fovDuration = std::max(fovDuration, duration);
		fovOffsetDegrees = std::max(fovOffsetDegrees, offsetDegrees);
	}

	// Called by the active third-person camera once per frame. Positive values
	// widen the FOV, producing a short zoom-out kick.
	static float UpdateFovOffset()
	{
		if (fovTimer <= 0.0f && currentFovOffsetDegrees <= 0.0f) return 0.0f;

		const float deltaTime = std::max(Game::Time::unscaledDeltaTime, 0.0f);
		fovTimer = std::max(fovTimer - deltaTime, 0.0f);

		// Spend the first part easing toward the wider FOV, then ease back. A new
		// request only extends the timer; it never snaps the current value.
		const bool expanding =
			fovDuration > 0.0f && fovTimer > fovDuration * 0.55f;
		const float target = expanding ? fovOffsetDegrees : 0.0f;
		const float response = expanding ? 18.0f : 10.0f;
		const float blend = 1.0f - expf(-response * deltaTime);
		currentFovOffsetDegrees +=
			(target - currentFovOffsetDegrees) * blend;

		if (fovTimer <= 0.0f && currentFovOffsetDegrees < 0.01f)
		{
			fovDuration = 0.0f;
			fovOffsetDegrees = 0.0f;
			currentFovOffsetDegrees = 0.0f;
		}
		return currentFovOffsetDegrees;
	}

    static void Update(Camera& camera,
        const Vector3& eye,
        const Vector3& focus,
        const Vector3& up = {0, 1, 0});

	bool IsPlaying() const { return isPlaying; }

private:
    inline static bool  isPlaying    = false;
    inline static float timer        = 0.0f;
    inline static float maxIntensity = 0.0f;
	inline static float fovTimer = 0.0f;
	inline static float fovDuration = 0.0f;
	inline static float fovOffsetDegrees = 0.0f;
	inline static float currentFovOffsetDegrees = 0.0f;
};
