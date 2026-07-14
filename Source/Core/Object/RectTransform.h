// RectTransform

#pragma once

#include "Core/Foundation/Common.h"

class Widget;

class RectTransform {
public:
    Vector2 position;
    float angle;
    Vector2 size;
    Vector2 anchor; // 0.0=left/top, 0.5=center, 1.0=right/bottom

    Widget* owner = nullptr;

    RectTransform(const Vector2& position = Vector2::Zero, float angle = 0, const Vector2& size = Vector2(100, 100), const Vector2 anchor = Vector2::Zero);

    static RectTransform FromPosition(const Vector2& position);
    static RectTransform FromAngle(const float angle);
    static RectTransform FromSize(const Vector2& size);
    static RectTransform FromSize(float size);

    void Update();
};