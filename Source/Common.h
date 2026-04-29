// Common.h
#pragma once

// 基本
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
#include <shellapi.h>
#include <wrl.h>

// imgui
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>

// Shader
#define CONCAT_INNER(a, b) a##b
#define CONCAT(a, b) CONCAT_INNER(a, b)
#define DUMMY CONCAT(__DUMMY__, __LINE__)

// Cereal
#include <cereal/cereal.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

// Json
#include <json.hpp>
using namespace nlohmann;

// DirectX
#include <DirectXTex.h>
#include <WICTextureLoader.h>
#include <DDSTextureLoader.h>
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

// 環境依存ベクトル暗黙変換
#define VEC(v) {((v).x),((v).y),((v).z)}
