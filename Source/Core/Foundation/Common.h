#pragma once

#include <Windows.h>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <cstddef>
#include <cctype>
#include <cwctype>
#include <filesystem>
#include <string>
#include <fstream>
#include <stdexcept>
#include <vector>

inline std::wstring Utf8ToWide(const std::string& text)
{
	if (text.empty()) return {};
	const int length =
		MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
	if (length <= 0) return std::wstring(text.begin(), text.end());
	std::wstring result(static_cast<size_t>(length), L'\0');
	MultiByteToWideChar(
		CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), length);
	return result;
}

// Convert a string to lowercase
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

// Convert a string to uppercase
inline std::wstring ToUpperWString(std::wstring text)
{
    std::transform(
        text.begin(),
        text.end(),
        text.begin(),
        [](wchar_t c) { return static_cast<wchar_t>(std::towupper(c)); }
    );

    return text;
}

// Convert a string to lowercase
inline std::string ToLowerString(std::string text)
{
    std::transform(
        text.begin(),
        text.end(),
        text.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); }
    );

    return text;
}

// Convert a string to uppercase
inline std::string ToUpperString(std::string text)
{
    std::transform(
        text.begin(),
        text.end(),
        text.begin(),
        [](unsigned char c) { return static_cast<char>(std::toupper(c)); }
    );

    return text;
}

inline bool ReadBinaryFile(const std::filesystem::path& path, std::vector<uint8_t>& outData)
{
	outData.clear();

	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file) return false;

	std::streamsize size = file.tellg();
	if (size <= 0) return false;

	outData.resize(static_cast<size_t>(size));
	file.seekg(0, std::ios::beg);
	file.read(reinterpret_cast<char*>(outData.data()), size);

	if (file) return true;

	outData.clear();
	return false;
}

// Generate a random
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

// Convert a 32-bit RGBA color to a Color object
inline Color ColorFromRGBA(uint32_t rgba)
{
    return Color(
        ((rgba >> 24) & 0xFF) / 255.0f,
        ((rgba >> 16) & 0xFF) / 255.0f,
        ((rgba >>  8) & 0xFF) / 255.0f,
        ((rgba >>  0) & 0xFF) / 255.0f
    );
}

// ピクセル座標へ変換
inline bool WorldToScreen(const Vector3& worldPosition, const Matrix& viewProjection,
	const Vector2& viewportSize, Vector2& screenPosition)
{
	if (!std::isfinite(viewportSize.x) || !std::isfinite(viewportSize.y)) return false;
	if (viewportSize.x <= 0.0f || viewportSize.y <= 0.0f) return false;

	// 1. ワールド座標を、カメラの向きと遠近法を反映した座標へ変換する。
	const Vector4 position(worldPosition.x, worldPosition.y, worldPosition.z, 1.0f);
	const Vector4 clipPosition = Vector4::Transform(position, viewProjection);

	const bool finitePosition = std::isfinite(clipPosition.x) && std::isfinite(clipPosition.y) &&
		std::isfinite(clipPosition.z) && std::isfinite(clipPosition.w);
	if (!finitePosition) return false;
	if (clipPosition.w <= eps) return false; // カメラの背後、または近すぎる位置。

	// 2. wで割り、画面内が横・縦ともに -1～1 になる座標へ変換する。
	const float normalizedX = clipPosition.x / clipPosition.w;
	const float normalizedY = clipPosition.y / clipPosition.w;
	const float normalizedDepth = clipPosition.z / clipPosition.w;
	if (normalizedDepth < 0.0f || normalizedDepth > 1.0f) return false;

	// 3. 画面左上を原点とするピクセル座標へ変換する。Yは上下を反転する。
	const float horizontalRatio = (normalizedX + 1.0f) * 0.5f;
	const float verticalRatio = (1.0f - normalizedY) * 0.5f;
	screenPosition.x = horizontalRatio * viewportSize.x;
	screenPosition.y = verticalRatio * viewportSize.y;
	return true;
}

#include <magic_enum/magic_enum_all.hpp>
