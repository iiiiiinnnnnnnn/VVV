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
		const float fill = std::clamp(horizontalFill, 0.0f, 1.0f);
		if (fill <= 0.0f) return;

		Game::Graphics::Instance().GetSpriteRenderer()->Draw(
			shaderId, texture,
			{ x, y, 0.0f },
			{widget->rect.size.x * fill, widget->rect.size.y},
			{ 0.0f, 0.0f },
			{(float)texture->GetWidth() * fill, (float)texture->GetHeight()},
			widget->rect.angle,
			color,
			shaderParameters);
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
	if (shaderId == SpriteShaderId::Vignette)
	{
		ImGui::ColorEdit3("Color", &color.x);
		ImGui::SliderFloat("Vignette Strength", &color.w, 0.0f, 1.0f);
		if (ImGui::SliderFloat("Vignette Range", &shaderParameters.x, 0.001f, 1.0f))
			shaderParameters.y = std::min(shaderParameters.y, shaderParameters.x);
		ImGui::SliderFloat("Vignette Softness", &shaderParameters.y,
			0.001f, shaderParameters.x);
	}
	else
	{
		ImGui::ColorEdit4("Color", &color.x);
	}
	ImGui::SliderFloat("Horizontal Fill", &horizontalFill, 0.0f, 1.0f);
}
