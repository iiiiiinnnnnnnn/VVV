#include "Gameplay/Scene/CameraEffectController.h"
#include "Application/Time/GameTime.h"

void CameraEffectController::Request(float duration, float intensity)
{
    timer.Extend(duration);
    maxIntensity  = std::max(maxIntensity, intensity);
    isPlaying        = true;
}

void CameraEffectController::Update(Camera& camera, const Vector3& eye, const Vector3& focus, const Vector3& up)
{
    if (!isPlaying)
    {
        // シェイクなし：そのまま SetLookAt
        camera.SetLookAt(eye, focus, up);
        return;
    }

    timer.Update(Game::Time::unscaledDeltaTime);
    if (timer.GetRemaining() <= 0.0f)
    {
        timer.Stop();
        maxIntensity = 0.0f;
        isPlaying = false;
        camera.SetLookAt(eye, focus, up);
        return;
    }

    // 時間経過で強度を減衰
    float ratio = timer.GetRemaining() / std::max(timer.GetRemaining() + Game::Time::unscaledDeltaTime, 0.0001f);
    float intensity = maxIntensity * ratio;

    // ランダムオフセット（右・上方向のみ。前後はほぼ気にならないので省略）
    float ox = Random::Range(-1.0f, 1.0f) * intensity;
    float oy = Random::Range(-1.0f, 1.0f) * intensity;

    Vector3 shakeOffset = camera.GetRight() * ox + camera.GetUp() * oy;

    camera.SetLookAt(eye + shakeOffset, focus + shakeOffset, up);
}