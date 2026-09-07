#include "Gameplay/Scene/HitStop.h"
#include "Core/Foundation/Common.h"
#include "Application/Time/GameTime.h"

void HitStop::Request(float duration)
{
    // すでに止まっている場合は長い方を優先
    timer = std::max(timer, duration);

    if (!isPlaying)
    {
        isPlaying = true;
        Game::Time::scale = 0.0f;
    }
}

void HitStop::Update()
{
    if (!isPlaying) return;

    timer -= Game::Time::unscaledDeltaTime;
    if (timer <= 0.0f)
    {
        timer  = 0.0f;
        isPlaying = false;
        Game::Time::scale = 1.0f;
    }
}