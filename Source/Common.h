// Common.h
#pragma once

// standard
#include <time.h> 
#include <ctime>
#include <vector>
#include <fstream>
#include <sstream>
#include <memory>
#include <string>
#include <algorithm>
#include <functional>
#include <filesystem>
#include <cmath>
#include <unordered_set>
#include <unordered_map>
#include <map>
#include <cstdint>
#include <exception>
#include <assert.h>
#include <tchar.h>
#include <iostream>
#include <variant>
#include <stack>
#include <queue>
#include <deque>
#include <cstring>
#include <cwctype>

// microsoft
#include <windows.h>
#include <wrl.h>

// imgui
#include <imgui.h>
#include <imgui_stdlib.h>

// Shader
#define CONCAT_INNER(a, b) a##b
#define CONCAT(a, b) CONCAT_INNER(a, b)
#define DUMMY CONCAT(__DUMMY__, __LINE__)

// DirectX
#include <d3d11.h> // directx
#include <d3dcompiler.h> // cso compiler
#include <DirectXMath.h> // direct x math
#include <SpriteBatch.h> // directx tool kit
#include <CommonStates.h> // directx tool kit

// SimpleMath
#include <SimpleMath.h>
using namespace DirectX::SimpleMath;
constexpr float eps = 1e-6f;
constexpr float RAD2DEG = 180.0f / DirectX::XM_PI;
constexpr float DEG2RAD = DirectX::XM_PI / 180.0f;

#define RAD(x) DirectX::XMConvertToRadians(x)
#define DEG(x) DirectX::XMConvertToDegrees(x)

static std::wstring ToLowerWString(std::wstring text)
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