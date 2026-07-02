// Benchmark.h

#pragma once

#include <windows.h>

class Benchmark
{
public:
    Benchmark()
    {
        QueryPerformanceFrequency(&ticksPerSecond);
        QueryPerformanceCounter(&startTicks);
        QueryPerformanceCounter(&currentTicks);
    }

    void Begin()
    {
        QueryPerformanceCounter(&startTicks);
    }

    float End()
    {
        QueryPerformanceCounter(&currentTicks);
        return static_cast<float>(currentTicks.QuadPart - startTicks.QuadPart) /
            static_cast<float>(ticksPerSecond.QuadPart);
    }

private:
    LARGE_INTEGER ticksPerSecond{};
    LARGE_INTEGER startTicks{};
    LARGE_INTEGER currentTicks{};
};