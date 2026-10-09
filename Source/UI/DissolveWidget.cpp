#include "UI/DissolveWidget.h"
#include "Rendering/Core/Graphics.h"
#include "Rendering/Renderer/SpriteRenderer.h"
#include "Resource/ResourceManager.h"

DissolveWidget::DissolveWidget(const std::string& name, const std::string& maskPath) : Widget(name)
{
	if (!maskPath.empty()) SetDissolveMask(maskPath);
}

void DissolveWidget::SetTexture(const std::string& path)
{
	texture = ResourceManager::Instance().LoadTexture(path);
}

void DissolveWidget::SetDissolveMask(const std::string& path)
{
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
	DrawSprite(texture, rect.position - rect.size * rect.anchor, rect.size,
		{static_cast<float>(texture->GetWidth()), static_cast<float>(texture->GetHeight())},
		rect.angle, Color(1, 1, 1, 1));
}

void DissolveWidget::OnDrawGUI()
{
	ImGui::SliderFloat((const char*)u8"ディソルブ影響度", &renderParams.dissolve.amount, 0.0f, 1.0f);
	ImGui::TextDisabled((const char*)u8"0: 全表示 / 1: 全消去");
	if (!renderParams.dissolve.mask)
		ImGui::TextDisabled((const char*)u8"マスク未設定");
}
