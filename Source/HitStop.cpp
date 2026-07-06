// HitStop.cpp

#include "HitStop.h"
#include "Common.h"
#include "GameTime.h"

void HitStop::Request(float duration)
{
    // ‚·‚Å‚É~‚Ü‚Á‚Ä‚¢‚éê‡‚Í’·‚¢•û‚ğ—Dæ
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