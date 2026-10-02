#pragma once

#include <vector>
#include <unordered_map>
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
	static std::shared_ptr<UIFont> Load(const std::string& path);

	bool IsReady() const { return !fontData.empty(); }
	float MeasureWidth(const std::string& text, float fontSize) const;
	void DrawFont(const std::string& text, const Vector2& position, const Vector2& boxSize,
		float fontSize, const Color& color, UITextAlignment alignment = UITextAlignment::Left) const;

private:
	struct Glyph
	{
		std::shared_ptr<Texture> texture;
		Vector2 sourcePosition = Vector2::Zero;
		Vector2 sourceSize = Vector2::Zero;
		Vector2 offset = Vector2::Zero;
		float advance = 0.0f;
	};

	UIFont(const std::string& path = "Resources/Font/Isometra.ttf");
	const Glyph& GetGlyph(unsigned int codepoint) const;
	static unsigned int NextCodepoint(const std::string& text, size_t& index);
	std::vector<unsigned char> fontData;
	mutable std::unordered_map<unsigned int, Glyph> glyphs;
	int fontOffset = 0;
	float bakedPixelSize = 64.0f;
	float baseline = 51.0f;
	float lineHeight = 64.0f;
};
