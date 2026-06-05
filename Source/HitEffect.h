#pragma once

#include "GameTime.h"
#include "Camera.h"
#include "Random.h"

// ─────────────────────────────────────────────────────────────────
// ヒットストップ
// 使い方：
//   HitStop::Request(0.1f);          // 0.1秒止める
//   HitStop::Update();               // 毎フレーム Update() で呼ぶ
// ─────────────────────────────────────────────────────────────────
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

    // 毎フレーム呼ぶ
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


// ─────────────────────────────────────────────────────────────────
// カメラシェイク
// 使い方：
//   CameraShake::Request(0.3f, 0.2f);         // 0.3秒、強度0.2
//   CameraShake::Update(camera, eye, focus);   // 毎フレーム呼ぶ
//   // ↑ eye/focus はカメラコントローラーが計算した「本来の値」を渡す
// ─────────────────────────────────────────────────────────────────
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

    // 毎フレーム呼ぶ
    // camera  : シーンのカメラ
    // eye     : カメラコントローラーが決めた本来の視点
    // focus   : カメラコントローラーが決めた本来の注視点
    // up      : 上ベクトル（通常 {0,1,0}）
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
