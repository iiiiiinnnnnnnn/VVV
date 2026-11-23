#include "UI/SpriteWidget.h"
#include <ImGuiToolKit/Color.h>
#include "Resource/ResourceManager.h"
#include "Rendering/Core/Graphics.h"

SpriteWidget::SpriteWidget(const std::string& name, std::filesystem::path spritePath,
	SpriteShaderId shaderId, const Color& color, SpriteRenderParams renderParams)
	: Widget(name), spritePath(std::move(spritePath)),
	shaderId(shaderId), color(color), renderParams(std::move(renderParams))
{
}

SpriteWidget::SpriteWidget(const std::string& name, std::shared_ptr<Texture> texture,
	SpriteShaderId shaderId, const Color& color, SpriteRenderParams renderParams)
	: Widget(name), texture(std::move(texture)), shaderId(shaderId),
	color(color), renderParams(std::move(renderParams))
{
}

void SpriteWidget::OnAwake()
{
	if (!texture && !spritePath.empty())
		texture = ResourceManager::Instance().LoadTexture(spritePath.generic_string());
}

void SpriteWidget::OnRender(const RenderContext&)
{
	DrawSprite(Vector2::Zero, color, shaderId, &renderParams);
}

void SpriteWidget::DrawShadow(const RenderContext&, const Vector2& offset, const Color& shadowColor)
{
	Color tint = shadowColor;
	tint.w *= color.w;
	DrawSprite(offset, tint, SpriteShaderId::Basic, nullptr);
}

void SpriteWidget::DrawSprite(const Vector2& offset, const Color& drawColor,
	SpriteShaderId drawShader, const SpriteRenderParams* params)
{
	if (!texture || drawColor.w <= 0.0f) return;
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
		drawShader, texture,
		{offset.x, offset.y, 0.0f},
		{rect.size.x * fill, rect.size.y},
		sourceTopLeft,
		{sourceExtent.x * fill, sourceExtent.y},
		0.0f,
		drawColor,
		params);
}

void SpriteWidget::OnDrawGUI()
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
		ImGuiToolKit::ColorEdit4("Color", &color.x);
		ImGui::DragFloat2("Vignette Center", &renderParams.vignette.center.x, 0.01f, 0.0f, 1.0f);
		ImGui::SliderFloat("Vignette Intensity", &renderParams.vignette.intensity, 0.0f, 5.0f);
		ImGui::SliderFloat("Vignette Smoothness", &renderParams.vignette.smoothness, 0.01f, 10.0f);
		ImGui::Checkbox("Vignette Rounded", &renderParams.vignette.rounded);
		ImGui::SliderFloat("Vignette Roundness", &renderParams.vignette.roundness, 0.1f, 8.0f);
	}
	else
	{
		ImGuiToolKit::ColorEdit4("Color", &color.x);
	}
	ImGui::SliderFloat("Horizontal Fill", &horizontalFill, 0.0f, 1.0f);
}
