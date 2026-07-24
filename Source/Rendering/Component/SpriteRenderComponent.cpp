// SpriteRenderComponent.cpp

#include "Rendering/Component/SpriteRenderComponent.h"
#include "Rendering/Core/Graphics.h"
#include "UI/Widget.h"
#include "IconsFontAwesome5.h"

SpriteRenderComponent::SpriteRenderComponent(
	Object* owner,
	std::shared_ptr<Texture> texture,
	SpriteShaderId shaderId,
	const Color& color)
	: Component(owner), texture(texture), shaderId(shaderId), color(color)
{
	// エラー用
	dynamic_cast<Widget*>(owner);
}

void SpriteRenderComponent::Update()
{
}

void SpriteRenderComponent::Render(const RenderContext& rc)
{
	Widget* widget = dynamic_cast<Widget*>(owner);
	if (texture)
	{
		// anchor分だけpositionをオフセット
		float x = widget->rect.position.x - widget->rect.size.x * widget->rect.anchor.x;
		float y = widget->rect.position.y - widget->rect.size.y * widget->rect.anchor.y;

		Game::Graphics::Instance().GetSpriteRenderer()->Draw(
			shaderId, texture,
			{ x, y, 0.0f },
			widget->rect.size,
			{ 0.0f, 0.0f },
			{ (float)texture->GetWidth(), (float)texture->GetHeight() },
			widget->rect.angle,
			color);
	}
}

void SpriteRenderComponent::DrawGUI()
{
	if (texture)
	{
		ImGui::Text("Texture: %s", texture->GetShaderResourceView() ? "Loaded" : "Not Loaded");
	}
	else
	{
		ImGui::Text("Texture: None");
	}
	ImGui::ColorEdit4("Color", &color.x);
}
