// HitEffect.h

#pragma once

#include "GameTime.h"
#include "Camera.h"
#include "Random.h"

#include <mutex>

class HitStop
{
public:
    // ヒットストップをリクエスト（秒数は unscaledDeltaTime 基準）
    static void Request(float duration)
    {
        // すでに止まっている場合は長い方を優先
        timer = max(timer, duration);

        if (!active)
        {
            active = true;
            Game::Time::scale = 0.0f;
        }
    }

    static void Update()
    {
        if (!active) return;

        timer -= Game::Time::unscaledDeltaTime;
        if (timer <= 0.0f)
        {
            timer  = 0.0f;
            active = false;
            Game::Time::scale = 1.0f;
        }
    }

    static bool IsActive() { return active; }

private:
    inline static bool  active = false;
    inline static float timer  = 0.0f;
};


class CameraShake
{
public:
    // シェイクをリクエスト
    // duration : 継続時間（秒）
    // intensity: 最大ブレ幅（ワールド単位）
    static void Request(float duration, float intensity)
    {
        timer         = max(timer, duration);
        maxIntensity  = max(maxIntensity, intensity);
        active        = true;
    }

    static void Update(Camera& camera,
                       const Vector3& eye,
                       const Vector3& focus,
                       const Vector3& up = { 0, 1, 0 })
    {
        if (!active)
        {
            // シェイクなし：そのまま SetLookAt
            camera.SetLookAt(eye, focus, up);
            return;
        }

        timer -= Game::Time::unscaledDeltaTime;
        if (timer <= 0.0f)
        {
            timer        = 0.0f;
            maxIntensity = 0.0f;
            active       = false;
            camera.SetLookAt(eye, focus, up);
            return;
        }

        // 時間経過で強度を減衰
        float ratio     = timer / max(timer + Game::Time::unscaledDeltaTime, 0.0001f);
        float intensity = maxIntensity * ratio;

        // ランダムオフセット（右・上方向のみ。前後はほぼ気にならないので省略）
        float ox = Random::Range(-1.0f, 1.0f) * intensity;
        float oy = Random::Range(-1.0f, 1.0f) * intensity;

        Vector3 shakeOffset = camera.GetRight() * ox + camera.GetUp() * oy;

        camera.SetLookAt(eye + shakeOffset, focus + shakeOffset, up);
    }

    static bool IsActive() { return active; }

private:
    inline static bool  active       = false;
    inline static float timer        = 0.0f;
    inline static float maxIntensity = 0.0f;
};

class DamageVignette
{
public:
    static void Init(ID3D11Device* device)
    {
        std::call_once(initFlag, []()
        {
            dummyTexture = std::make_shared<Texture>(Color(1, 1, 1, 1));
        });
    }

    static void Request(float duration)
    {
        timer = max(timer, duration);
        active = true;
    }

    static void Update(SpriteRenderer* renderer,
                       float screenW = 1280.0f, float screenH = 720.0f)
    {
        if (!active) return;

        timer -= Game::Time::unscaledDeltaTime;
        if (timer <= 0.0f)
        {
            timer  = 0.0f;
            active = false;
            return;
        }

        float alpha = std::clamp(timer / 0.5f, 0.0f, 1.0f);

        ShaderParamList params;
        params.push_back({ "color", Color(1, 0, 0, alpha) });

        renderer->Draw(
            SpriteShaderId::Vignette,
            dummyTexture,
            Vector3(0, 0, 0),
            Vector2(screenW, screenH),
            Vector2(0, 0),
            Vector2(1, 1),
            0.0f,
            params);
    }

    static bool IsActive() { return active; }

private:
    inline static bool  active = false;
    inline static float timer  = 0.0f;

    inline static std::once_flag initFlag;
    inline static std::shared_ptr<Texture> dummyTexture;
};