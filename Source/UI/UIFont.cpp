#include "UI/UIFont.h"

#include <algorithm>
#include <fstream>
#include <vector>

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

UIFont::UIFont()
{
	std::ifstream stream("Resources/Font/ArialUni.ttf", std::ios::binary | std::ios::ate);
	if (!stream) return;
	const std::streamsize length = stream.tellg();
	if (length <= 0) return;
	stream.seekg(0, std::ios::beg);
	std::vector<unsigned char> fontData(static_cast<size_t>(length));
	if (!stream.read(reinterpret_cast<char*>(fontData.data()), length)) return;

	constexpr int atlasWidth = 1024;
	constexpr int atlasHeight = 512;
	std::vector<unsigned char> mono(atlasWidth * atlasHeight, 0);
	std::array<stbtt_bakedchar, 95> baked{};
	if (stbtt_BakeFontBitmap(fontData.data(), 0, bakedPixelSize, mono.data(),
		atlasWidth, atlasHeight, 32, static_cast<int>(baked.size()), baked.data()) <= 0)
		return;

	std::vector<unsigned char> rgba(atlasWidth * atlasHeight * 4, 255);
	for (size_t i = 0; i < mono.size(); ++i)
		rgba[i * 4 + 3] = mono[i];

	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = atlasWidth;
	desc.Height = atlasHeight;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_IMMUTABLE;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	D3D11_SUBRESOURCE_DATA data{};
	data.pSysMem = rgba.data();
	data.SysMemPitch = atlasWidth * 4;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
	ID3D11Device* device = Game::Graphics::Instance().GetDevice();
	if (FAILED(device->CreateTexture2D(&desc, &data, texture.GetAddressOf()))) return;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
	if (FAILED(device->CreateShaderResourceView(texture.Get(), nullptr, srv.GetAddressOf()))) return;
	atlas = std::make_shared<Texture>(srv.Get(), desc);

	for (size_t i = 0; i < baked.size(); ++i)
	{
		const stbtt_bakedchar& source = baked[i];
		glyphs[i].sourcePosition = {static_cast<float>(source.x0), static_cast<float>(source.y0)};
		glyphs[i].sourceSize = {static_cast<float>(source.x1 - source.x0),
			static_cast<float>(source.y1 - source.y0)};
		glyphs[i].offset = {source.xoff, source.yoff};
		glyphs[i].advance = source.xadvance;
	}
}

float UIFont::MeasureWidth(const std::string& text, float fontSize) const
{
	const float scale = fontSize / bakedPixelSize;
	float width = 0.0f;
	for (unsigned char c : text)
		if (c >= 32 && c <= 126) width += glyphs[c - 32].advance * scale;
	return width;
}

void UIFont::DrawText(const std::string& text, const Vector2& position, const Vector2& boxSize,
	float fontSize, const Color& color, UITextAlignment alignment) const
{
	if (!atlas || fontSize <= 0.0f) return;
	const float scale = fontSize / bakedPixelSize;
	const float width = MeasureWidth(text, fontSize);
	float penX = position.x;
	if (alignment == UITextAlignment::Center) penX += (boxSize.x - width) * 0.5f;
	else if (alignment == UITextAlignment::Right) penX += boxSize.x - width;
	const float penY = position.y + (boxSize.y - fontSize) * 0.5f + baseline * scale;

	SpriteRenderer* renderer = Game::Graphics::Instance().GetSpriteRenderer();
	for (unsigned char c : text)
	{
		if (c < 32 || c > 126) continue;
		const Glyph& glyph = glyphs[c - 32];
		if (glyph.sourceSize.x > 0.0f && glyph.sourceSize.y > 0.0f)
		{
			renderer->Draw(SpriteShaderId::Basic, atlas,
				{penX + glyph.offset.x * scale, penY + glyph.offset.y * scale, 0.0f},
				{glyph.sourceSize.x * scale, glyph.sourceSize.y * scale},
				glyph.sourcePosition, glyph.sourceSize, 0.0f, color);
		}
		penX += glyph.advance * scale;
	}
}
