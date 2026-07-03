// SceneEffect.h

#pragma once
#include <d3d11.h>
#include <wrl.h>

#include <algorithm>
#include <cmath>
#include <memory>

#include "GameTime.h"
#include "Camera.h"
#include "Random.h"
#include "Graphics.h"
#include <mutex>
#include "SpriteRenderer.h"

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

class CameraThreaten
{
public:
    static void Request(float duration, float fovMultiplier = 1.5f)
    {
        // すでに威嚇演出中、または戻り中なら無視
        if (active || intensity > 0.001f)
        {
            return;
        }

        timer = duration;
        multiplier = fovMultiplier;
        active = true;
    }

    static void ForceRequest(float duration, float fovMultiplier = 1.5f)
    {
        timer = duration;
        multiplier = fovMultiplier;
        active = true;
    }

    static void Update()
    {
        const float dt = Game::Time::unscaledDeltaTime;

        if (active)
        {
            timer -= dt;

            if (timer <= 0.0f)
            {
                timer = 0.0f;
                active = false;
            }
        }

        const float targetIntensity = active ? 1.0f : 0.0f;
        const float speed = active ? easeInSpeed : easeOutSpeed;
        const float t = 1.0f - expf(-speed * dt);

        intensity = std::lerp(intensity, targetIntensity, t);

        if (!active && intensity < 0.001f)
        {
            intensity = 0.0f;
            multiplier = 1.0f;
        }
    }

    static float GetFovMultiplier()
    {
        return std::lerp(1.0f, multiplier, EaseOutCubic(intensity));
    }

    static float GetTimer()
    {
        return timer;
    }

    static float GetIntensity()
    {
        return intensity;
    }

    static bool IsActive()
    {
        return active || intensity > 0.001f;
    }

private:
    static float EaseOutCubic(float x)
    {
        x = std::clamp(x, 0.0f, 1.0f);
        const float inv = 1.0f - x;
        return 1.0f - inv * inv * inv;
    }

private:
    inline static bool active = false;
    inline static float timer = 0.0f;
    inline static float multiplier = 1.0f;
    inline static float intensity = 0.0f;

    inline static float easeInSpeed = 2.0f;
    inline static float easeOutSpeed = 1.5f;
};

class ThreatenLines
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
        if (active || intensity > 0.001f)
        {
            return;
        }

        timer = duration;
        effectTime = 0.0f;
        randomSeed = Random::Range(0.0f, 1000.0f);
        active = true;
    }

    static void Update(SpriteRenderer* renderer,
        float screenW = Game::Graphics::ScreenWidth,
        float screenH = Game::Graphics::ScreenHeight)
    {
        const float dt = Game::Time::unscaledDeltaTime;

        effectTime += dt;

        if (active)
        {
            timer -= dt;

            if (timer <= 0.0f)
            {
                timer = 0.0f;
                active = false;
            }
        }

        const float targetIntensity = active ? 1.0f : 0.0f;
        const float speed = active ? easeInSpeed : easeOutSpeed;
        const float t = 1.0f - expf(-speed * dt);

        intensity = std::lerp(intensity, targetIntensity, t);

        if (!active && intensity < 0.001f)
        {
            intensity = 0.0f;
        }

        if (!renderer) return;
        if (!dummyTexture) return;
        if (intensity <= 0.001f) return;

        const float eased = EaseOutQuad(intensity);

        ShaderParamList params;
        params.push_back({ "color", Color(1.0f, 0, 0, 0.35f) });
        params.push_back({ "center", Vector2(0.5f, 0.5f) });
        params.push_back({ "screenAspect", screenW / screenH });
        params.push_back({ "lineCount", 64.0f });
        params.push_back({ "lineWidth", 0.9f });
        params.push_back({ "softness", 0.04f });
        params.push_back({ "innerRadius", 0.18f });
        params.push_back({ "outerRadius", 0.98f });
        params.push_back({ "randomStrength", 0.45f });
        params.push_back({ "randomSeed", randomSeed });
        params.push_back({ "rotationSpeed", 0.25f });
        params.push_back({ "rotation", 0.0f });

        // イージングされた表示強度
        params.push_back({ "alphaMultiplier", eased });

        // ランダム化
        params.push_back({ "time", Game::Time::time });
        params.push_back({ "randomChangeSpeed", 14.0f });
        params.push_back({ "rotationSpeed", 0.2f });
        params.push_back({ "noiseScroll", 0.0f });

        renderer->Draw(
            SpriteShaderId::Basic,
            dummyTexture,
            Vector3(0, 0, 0),
            Vector2(screenW, screenH),
            Vector2(0, 0),
            Vector2(1, 1),
            0.0f,
            params);
    }

    static bool IsActive()
    {
        return active;
    }

    static float GetIntensity()
    {
        return intensity;
    }

private:
    static float EaseOutQuad(float x)
    {
        x = std::clamp(x, 0.0f, 1.0f);
        return 1.0f - (1.0f - x) * (1.0f - x);
    }

    inline static bool active = false;
    inline static float timer = 0.0f;
    inline static float intensity = 0.0f;
    inline static float effectTime = 0.0f;
    inline static float randomSeed = 0.0f;

    inline static float easeInSpeed = 3.5f;
    inline static float easeOutSpeed = 2.0f;

    inline static std::once_flag initFlag;
    inline static std::shared_ptr<Texture> dummyTexture;
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
        float screenW = Game::Graphics::ScreenWidth,
        float screenH = Game::Graphics::ScreenHeight)
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
            SpriteShaderId::Basic,
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
