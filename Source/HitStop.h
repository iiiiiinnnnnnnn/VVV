// HitStop.h

#pragma once

class HitStop
{
public:
    static void Request(float duration);

    static void Update();

    static bool IsPlaying() { return isPlaying; }

private:
    inline static bool  isPlaying = false;
    inline static float timer  = 0.0f;
};
