#include "Rendering/Component/SpriteRenderComponent.h"
#include "Rendering/Core/Graphics.h"
#include "UI/Widget.h"
#include "IconsFontAwesome5.h"

SpriteRenderComponent::SpriteRenderComponent(
	Object* owner,
	std::shared_ptr<Texture> texture,
	SpriteShaderId shaderId,
	const Color& color,
	SpriteRenderParams renderParams)
	: Component(owner), texture(texture), shaderId(shaderId), color(color),
	renderParams(std::move(renderParams))
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
		const Vector2 textureSize(
			static_cast<float>(texture->GetWidth()),
			static_cast<float>(texture->GetHeight()));
		Vector2 sourceTopLeft = useSourceRect ? sourcePosition : Vector2::Zero;
		Vector2 sourceExtent = useSourceRect ? sourceSize : textureSize;
		sourceTopLeft.x = std::clamp(sourceTopLeft.x, 0.0f, textureSize.x);
		sourceTopLeft.y = std::clamp(sourceTopLeft.y, 0.0f, textureSize.y);
		sourceExtent.x = std::clamp(sourceExtent.x, 0.0f, textureSize.x - sourceTopLeft.x);
		sourceExtent.y = std::clamp(sourceExtent.y, 0.0f, textureSize.y - sourceTopLeft.y);
		if (sourceExtent.x <= 0.0f || sourceExtent.y <= 0.0f) return;

		Game::Graphics::Instance().GetSpriteRenderer()->Draw(
			shaderId, texture,
			{ x, y, 0.0f },
			{widget->rect.size.x * fill, widget->rect.size.y},
			sourceTopLeft,
			{sourceExtent.x * fill, sourceExtent.y},
			widget->rect.angle,
			color,
			&renderParams);
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
	if (shaderId == SpriteShaderId::Vignette || shaderId == SpriteShaderId::VignetteOverlay)
	{
		ImGui::ColorEdit3("Color", &color.x);
		ImGui::SliderFloat("Vignette Strength", &color.w, 0.0f, 1.0f);
		if (ImGui::SliderFloat(
			"Vignette Range", &renderParams.vignette.range, 0.001f, 1.0f))
		{
			renderParams.vignette.softness = std::min(
				renderParams.vignette.softness, renderParams.vignette.range);
		}
		ImGui::SliderFloat("Vignette Softness", &renderParams.vignette.softness,
			0.001f, renderParams.vignette.range);
	}
	else
	{
		ImGui::ColorEdit4("Color", &color.x);
	}
	ImGui::SliderFloat("Horizontal Fill", &horizontalFill, 0.0f, 1.0f);
}
