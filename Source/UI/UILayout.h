#pragma once

#include <algorithm>

#include "Rendering/Core/Graphics.h"
#include "Rendering/Component/SpriteRenderComponent.h"
#include "UI/Button.h"
#include "UI/GaugeHUD.h"
#include "UI/TextWidget.h"

// 画面比率の座標をピクセルへ変換する。anchorはWidget内の基準点
class UILayout
{
public:
	static float Scale()
	{
		return std::clamp(Game::Graphics::ScreenHeight / 1080.0f, 0.65f, 1.5f);
	}

	// テキストの配置と、解像度に合わせた文字サイズを反映する。
	static void Apply(Widget& widget, Vector2 position, Vector2 size,
		float fontSize = 0, Vector2 offset = Vector2::Zero, Vector2 anchor = Vector2::Zero)
	{
		const Vector2 screen(Game::Graphics::ScreenWidth, Game::Graphics::ScreenHeight);
		widget.rect.position = position * screen + offset;
		widget.rect.size = size * screen;
		widget.rect.anchor = anchor;
		widget.rect.angle = 0;
		if (fontSize <= 0) return;
		if (auto* text = dynamic_cast<TextWidget*>(&widget))
			text->SetFontSize(fontSize * Scale());
		if (auto* button = dynamic_cast<Button*>(&widget))
			button->SetFontSize(fontSize * Scale());
	}

	static void Apply(const std::shared_ptr<Widget>& widget, Vector2 position, Vector2 size,
		float fontSize = 0, Vector2 offset = Vector2::Zero, Vector2 anchor = Vector2::Zero)
	{
		if (widget) Apply(*widget, position, size, fontSize, offset, anchor);
	}

	// タイトルとポーズで共通の画像付きボタンサイズ。
	static void MenuButton(const std::shared_ptr<Button>& button, Vector2 position,
		float fontSize, Vector2 offset = Vector2::Zero)
	{
		const float height = Game::Graphics::ScreenHeight;
		if (!button || height <= 0.0f) return;
		Apply(button, position, {0.28f, 200.0f / height}, fontSize, offset, {0.5f, 0.5f});
	}

	// ロゴなどは指定領域に収め、元画像の縦横比を保つ。
	static void Image(Widget& widget, Vector2 position, Vector2 size,
		Vector2 offset = Vector2::Zero, Vector2 anchor = Vector2::Zero)
	{
		Apply(widget, position, size, 0, offset, anchor);
		const auto* sprite = widget.GetComponent<SpriteRenderComponent>();
		const auto* texture = sprite ? sprite->GetTexture() : nullptr;
		if (!texture || !texture->GetWidth() || !texture->GetHeight()) return;
		const float scale = std::max(0.0f, std::min(widget.rect.size.x / texture->GetWidth(),
			widget.rect.size.y / texture->GetHeight()));
		widget.rect.size = Vector2(static_cast<float>(texture->GetWidth()),
			static_cast<float>(texture->GetHeight())) * scale;
	}

	static void Image(const std::shared_ptr<Widget>& widget, Vector2 position, Vector2 size,
		Vector2 offset = Vector2::Zero, Vector2 anchor = Vector2::Zero)
	{
		if (widget) Image(*widget, position, size, offset, anchor);
	}

	// 装飾画像の透明余白を切り出す。
	static void Source(const std::shared_ptr<Widget>& widget, Vector2 position, Vector2 size)
	{
		if (!widget) return;
		if (auto* sprite = widget->GetComponent<SpriteRenderComponent>())
			sprite->SetSourceRect(position, size);
	}

	// ゲージ画像内の塗りつぶし領域は、ゲージに対する比率で調整する。
	static void Fill(const std::shared_ptr<GaugeHUD>& gauge, Vector2 position, Vector2 size)
	{
		if (!gauge) return;
		gauge->SetFillRect(position, size);
	}
};
