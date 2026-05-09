// Common.h
#pragma once

// Šî–{
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

// Json
//#include <json.hpp>
//using namespace nlohmann;

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

// ŠÂ‹«ˆË‘¶ˆÃ–Ù•ÏŠ·
#define VEC(v) { \
    static_cast<float>(((v)[0])), static_cast<float>(((v))[1]), static_cast<float>(((v)[2])) \
}
#define QUAT(q) { \
    static_cast<float>(((q)[0])), static_cast<float>(((q))[1]), static_cast<float>(((q))[2]), static_cast<float>(((q))[3]) \
}
#define MATRIX(m) { \
    static_cast<float>((m)[0]),  static_cast<float>((m)[1]),  static_cast<float>((m)[2]),  static_cast<float>((m)[3]), \
    static_cast<float>((m)[4]),  static_cast<float>((m)[5]),  static_cast<float>((m)[6]),  static_cast<float>((m)[7]), \
    static_cast<float>((m)[8]),  static_cast<float>((m)[9]),  static_cast<float>((m)[10]), static_cast<float>((m)[11]), \
    static_cast<float>((m)[12]), static_cast<float>((m)[13]), static_cast<float>((m)[14]), static_cast<float>((m)[15]) \
}
