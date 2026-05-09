// Widget.cpp

#include "Widget.h"

void Widget::Update(float elapsedTime)
{
    if (alpha != targetAlpha)
    {
        float diff = targetAlpha - alpha;
        float delta = fadeSpeed * elapsedTime;
        if (fabsf(diff) <= delta)
            alpha = targetAlpha;
        else
            alpha += (diff > 0.0f ? 1.0f : -1.0f) * delta;
    }
}
