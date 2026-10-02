#include "UI/UIFont.h"

#include <algorithm>
#include <fstream>
#include <vector>
#include <filesystem>
#include <mutex>
#include <unordered_map>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include "Rendering/Core/Graphics.h"
#include "Rendering/Renderer/SpriteRenderer.h"
#include "Resource/Texture.h"

UIFont& UIFont::Default()
{
	static UIFont instance;
	return instance;
}

std::shared_ptr<UIFont> UIFont::Load(const std::string& path)
{
	static std::mutex fontMutex;
	static std::unordered_map<std::string, std::shared_ptr<UIFont>> fonts;
	const std::string key = std::filesystem::path(path).lexically_normal().generic_string();
	std::lock_guard<std::mutex> lock(fontMutex);
	if (const auto it = fonts.find(key); it != fonts.end()) return it->second;
	auto font = std::shared_ptr<UIFont>(new UIFont(key));
	if (!font->IsReady()) return nullptr;
	fonts.emplace(key, font);
	return font;
}

UIFont::UIFont(const std::string& path)
{
	std::ifstream stream(path, std::ios::binary | std::ios::ate);
	if (!stream) return;
	const std::streamsize length = stream.tellg();
	if (length <= 0) return;
	stream.seekg(0, std::ios::beg);
	fontData.resize(static_cast<size_t>(length));
	if (!stream.read(reinterpret_cast<char*>(fontData.data()), length))
	{
		fontData.clear();
		return;
	}
	fontOffset = stbtt_GetFontOffsetForIndex(fontData.data(), 0);
	stbtt_fontinfo info{};
	if (fontOffset < 0 || !stbtt_InitFont(&info, fontData.data(), fontOffset))
	{
		fontData.clear();
		return;
	}
	int ascent, descent, gap;
	stbtt_GetFontVMetrics(&info, &ascent, &descent, &gap);
	const float scale = stbtt_ScaleForPixelHeight(&info, bakedPixelSize);
	baseline = ascent * scale;
	lineHeight = (ascent - descent + gap) * scale;
}

unsigned int UIFont::NextCodepoint(const std::string& text, size_t& index)
{
	const unsigned char first = static_cast<unsigned char>(text[index++]);
	if (first < 0x80) return first;
	const int count = first >= 0xF0 && first <= 0xF4 ? 3
		: first >= 0xE0 && first <= 0xEF ? 2 : first >= 0xC2 && first <= 0xDF ? 1 : 0;
	if (!count) return 0xFFFD;
	unsigned int value = first & (0x7F >> (count + 1));
	for (int i = 0; i < count; ++i)
	{
		if (index >= text.size()) return 0xFFFD;
		const unsigned char next = static_cast<unsigned char>(text[index]);
		if ((next & 0xC0) != 0x80) return 0xFFFD;
		++index;
		value = (value << 6) | (next & 0x3F);
	}
	if (value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF)
		|| (count == 1 && value < 0x80) || (count == 2 && value < 0x800)
		|| (count == 3 && value < 0x10000)) return 0xFFFD;
	return value;
}

const UIFont::Glyph& UIFont::GetGlyph(unsigned int codepoint) const
{
	if (const auto it = glyphs.find(codepoint); it != glyphs.end()) return it->second;
	Glyph glyph;
	stbtt_fontinfo info{};
	if (!IsReady() || !stbtt_InitFont(&info, fontData.data(), fontOffset))
		return glyphs.emplace(codepoint, glyph).first->second;
	const float scale = stbtt_ScaleForPixelHeight(&info, bakedPixelSize);
	int advance, bearing;
	stbtt_GetCodepointHMetrics(&info, static_cast<int>(codepoint), &advance, &bearing);
	glyph.advance = advance * scale;
	int width, height, offsetX, offsetY;
	unsigned char* bitmap = stbtt_GetCodepointBitmap(&info, scale, scale,
		static_cast<int>(codepoint), &width, &height, &offsetX, &offsetY);
	glyph.offset = {static_cast<float>(offsetX), static_cast<float>(offsetY)};
	glyph.sourceSize = {static_cast<float>(width), static_cast<float>(height)};
	if (bitmap && width > 0 && height > 0)
	{
		std::vector<unsigned char> rgba(static_cast<size_t>(width) * height * 4, 255);
		for (size_t i = 0; i < static_cast<size_t>(width) * height; ++i)
			rgba[i * 4 + 3] = bitmap[i];
		D3D11_TEXTURE2D_DESC desc{};
		desc.Width = width;
		desc.Height = height;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_IMMUTABLE;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		D3D11_SUBRESOURCE_DATA data{};
		data.pSysMem = rgba.data();
		data.SysMemPitch = width * 4;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
		auto* device = Game::Graphics::Instance().GetDevice();
		if (SUCCEEDED(device->CreateTexture2D(&desc, &data, texture.GetAddressOf()))
			&& SUCCEEDED(device->CreateShaderResourceView(texture.Get(), nullptr, srv.GetAddressOf())))
			glyph.texture = std::make_shared<Texture>(srv.Get(), desc);
	}
	stbtt_FreeBitmap(bitmap, nullptr);
	return glyphs.emplace(codepoint, std::move(glyph)).first->second;
}

float UIFont::MeasureWidth(const std::string& text, float fontSize) const
{
	if (!IsReady() || fontSize <= 0.0f) return 0.0f;
	const float scale = fontSize / bakedPixelSize;
	float width = 0.0f;
	float widest = 0.0f;
	for (size_t i = 0; i < text.size();)
	{
		const unsigned int c = NextCodepoint(text, i);
		if (c == '\n') { widest = std::max(widest, width); width = 0.0f; }
		else if (c == '\t') width += GetGlyph(' ').advance * scale * 4;
		else if (c >= 32) width += GetGlyph(c).advance * scale;
	}
	return std::max(widest, width);
}

void UIFont::DrawFont(const std::string& text, const Vector2& position, const Vector2& boxSize,
	float fontSize, const Color& color, UITextAlignment alignment) const
{
	if (!IsReady() || fontSize <= 0.0f) return;
	const float scale = fontSize / bakedPixelSize;
	const size_t lineCount = 1 + std::count(text.begin(), text.end(), '\n');
	float penY = position.y + (boxSize.y - fontSize - (lineCount - 1) * lineHeight * scale) * 0.5f
		+ baseline * scale;
	SpriteRenderer* renderer = Game::Graphics::Instance().GetSpriteRenderer();
	for (size_t start = 0; start <= text.size();)
	{
		const size_t end = text.find('\n', start);
		const std::string line = text.substr(start, end == std::string::npos ? end : end - start);
		const float width = MeasureWidth(line, fontSize);
		float penX = position.x;
		if (alignment == UITextAlignment::Center) penX += (boxSize.x - width) * 0.5f;
		else if (alignment == UITextAlignment::Right) penX += boxSize.x - width;
		for (size_t i = 0; i < line.size();)
		{
			const unsigned int c = NextCodepoint(line, i);
			if (c == '\t') { penX += GetGlyph(' ').advance * scale * 4; continue; }
			if (c < 32) continue;
			const Glyph& glyph = GetGlyph(c);
			if (glyph.texture)
				renderer->Draw(SpriteShaderId::Basic, glyph.texture,
					{penX + glyph.offset.x * scale, penY + glyph.offset.y * scale, 0.0f},
					glyph.sourceSize * scale, glyph.sourcePosition, glyph.sourceSize, 0.0f, color);
			penX += glyph.advance * scale;
		}
		if (end == std::string::npos) break;
		start = end + 1;
		penY += lineHeight * scale;
	}
}
