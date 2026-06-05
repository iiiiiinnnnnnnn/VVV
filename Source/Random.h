// Random.h

#pragma once

#include <cstdlib>

class Random
{
public:
    static float Range(float min, float max)
    {
        float t = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        return min + t * (max - min);
    }

    static int Range(int min, int max)
    {
        return min + rand() % (max - min + 1);
    }
};