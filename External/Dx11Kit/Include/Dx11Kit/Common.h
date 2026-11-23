#pragma once

#include <algorithm>
#include <array>
#include <cereal/cereal.hpp>
#include <cmath>
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
