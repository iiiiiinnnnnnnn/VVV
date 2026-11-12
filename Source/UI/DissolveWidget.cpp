#include "UI/DissolveWidget.h"
#include <ImGuiToolKit/Widgets.h>
#include "Rendering/Core/Graphics.h"
#include "Rendering/Renderer/SpriteRenderer.h"
#include "Resource/ResourceManager.h"

DissolveWidget::DissolveWidget(const std::string& name, const std::string& maskPath) : Widget(name), maskPath(maskPath)
{
}

void DissolveWidget::OnAwake()
{
	if (!maskPath.empty()) SetDissolveMask(maskPath);
	if (!texturePath.empty()) SetTexture(texturePath);
}

void DissolveWidget::SetTexture(const std::string& path)
{
	texturePath = path;
	if (!isAwake && !isAwaking) return;
	texture = ResourceManager::Instance().LoadTexture(path);
}

void DissolveWidget::SetDissolveMask(const std::string& path)
{
	maskPath = path;
	if (!isAwake && !isAwaking) return;
	renderParams.dissolve.mask = ResourceManager::Instance().LoadTexture(path);
}

void DissolveWidget::DrawSprite(const std::shared_ptr<Texture>& sprite, const Vector2& position,
	const Vector2& size, const Vector2& sourceSize, float angle, const Color& color)
{
	if (!sprite) return;
	Game::Graphics::Instance().GetSpriteRenderer()->Draw(
		renderParams.dissolve.mask ? SpriteShaderId::Dissolve : SpriteShaderId::Basic,
		sprite, {position.x, position.y, 0.0f}, size, Vector2::Zero, sourceSize,
		angle, color, &renderParams);
}

void DissolveWidget::OnRender(const RenderContext& rc)
{
	if (!texture) return;
	DrawSprite(texture, Vector2::Zero, rect.size,
		{static_cast<float>(texture->GetWidth()), static_cast<float>(texture->GetHeight())},
		0.0f, spriteColor);
}

void DissolveWidget::OnDrawGUI()
{
	ImGuiToolKit::SliderFloat(u8"ディソルブ影響度", &renderParams.dissolve.amount, 0.0f, 1.0f);
	ImGuiToolKit::TextDisabled(u8"0: 全表示 / 1: 全消去");
	ImGuiToolKit::DragFloat(u8"ノイズの表示範囲", &renderParams.dissolve.maskScale, 0.01f, 0.01f, 10.0f);
	ImGuiToolKit::DragFloat(u8"境界の柔らかさ", &renderParams.dissolve.softness, 0.005f, 0.0f, 0.5f);
	if (!renderParams.dissolve.mask)
		ImGuiToolKit::TextDisabled(u8"マスク未設定");
}
