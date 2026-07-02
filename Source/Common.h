// Common.h

#pragma once

#include <algorithm>
#include <cstdint>
#include <cwctype>
#include <string>
#include <fstream>

#include <DirectXMath.h>
#include <SimpleMath.h>

using DirectX::SimpleMath::Vector2;
using DirectX::SimpleMath::Vector3;
using DirectX::SimpleMath::Vector4;
using DirectX::SimpleMath::Matrix;
using DirectX::SimpleMath::Quaternion;
using DirectX::SimpleMath::Color;

constexpr float eps = 1e-6f;

#define RAD(x) DirectX::XMConvertToRadians(x)
#define DEG(x) DirectX::XMConvertToDegrees(x)

inline std::wstring ToLowerWString(std::wstring text)
{
    std::transform(
        text.begin(),
        text.end(),
        text.begin(),
        [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); }
    );

    return text;
}

inline Color ColorFromRGBA(uint32_t rgba)
{
    return Color(
        ((rgba >> 24) & 0xFF) / 255.0f,
        ((rgba >> 16) & 0xFF) / 255.0f,
        ((rgba >>  8) & 0xFF) / 255.0f,
        ((rgba >>  0) & 0xFF) / 255.0f
    );
}