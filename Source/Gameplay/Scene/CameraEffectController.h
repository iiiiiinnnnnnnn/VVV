// CameraEffectController.h

#pragma once

#include "Core/Foundation/Common.h"
#include "Gameplay/Camera/Camera.h"

class CameraEffectController
{
public:
    static void Request(float duration, float intensity);

    static void Update(Camera& camera,
        const Vector3& eye,
        const Vector3& focus,
        const Vector3& up = {0, 1, 0});

	bool IsPlaying() const { return isPlaying; }

private:
    inline static bool  isPlaying    = false;
    inline static float timer        = 0.0f;
    inline static float maxIntensity = 0.0f;
};
