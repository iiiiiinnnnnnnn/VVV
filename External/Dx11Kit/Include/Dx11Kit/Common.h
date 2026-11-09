#pragma once

#include <algorithm>
#include <array>
#include <cereal/cereal.hpp>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cwctype>
#include <d3d11.h>
#include <DirectXMath.h>
#include <DirectXTex.h>
#include <dxgi.h>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <SimpleMath.h>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <wrl.h>

using DirectX::SimpleMath::Color;
using DirectX::SimpleMath::Matrix;
using DirectX::SimpleMath::Quaternion;
using DirectX::SimpleMath::Vector2;
using DirectX::SimpleMath::Vector3;
using DirectX::SimpleMath::Vector4;

constexpr float eps = 1e-6f;

#define RAD(x) DirectX::XMConvertToRadians(x)
#define DEG(x) DirectX::XMConvertToDegrees(x)

// Convert a 32-bit RGBA color to a Color object
inline Color ColorFromRGBA(uint32_t rgba)
{
	return Color(
		((rgba >> 24) & 0xFF) / 255.0f,
		((rgba >> 16) & 0xFF) / 255.0f,
		((rgba >> 8) & 0xFF) / 255.0f,
		((rgba >> 0) & 0xFF) / 255.0f);
}