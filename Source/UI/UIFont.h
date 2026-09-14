#pragma once

#include <array>
#include <memory>
#include <string>

#include "Core/Foundation/Common.h"

class Texture;

enum class UITextAlignment
{
	Left,
	Center,
	Right
};

// TTFから作ったグリフアトラスを通常のSpriteRendererへ流すゲーム用フォント。
class UIFont
{
public:
	static UIFont& Default();

	bool IsReady() const { return atlas != nullptr; }
	float MeasureWidth(const std::string& text, float fontSize) const;
	void DrawText(const std::string& text, const Vector2& position, const Vector2& boxSize,
		float fontSize, const Color& color, UITextAlignment alignment = UITextAlignment::Left) const;

private:
	struct Glyph
	{
		Vector2 sourcePosition = Vector2::Zero;
		Vector2 sourceSize = Vector2::Zero;
		Vector2 offset = Vector2::Zero;
		float advance = 0.0f;
	};

	UIFont();
	std::shared_ptr<Texture> atlas;
	std::array<Glyph, 95> glyphs{};
	float bakedPixelSize = 64.0f;
	float baseline = 51.0f;
};
