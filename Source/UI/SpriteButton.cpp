#include "UI/SpriteButton.h"
#include <ImGuiToolKit/Color.h>

#include <algorithm>
#include <cmath>

#include "Application/Time/GameTime.h"
#include "Rendering/Core/Graphics.h"
#include "Rendering/Renderer/SpriteRenderer.h"
#include "Resource/ResourceManager.h"
#include "Resource/Texture.h"
#include "UI/EventTrigger.h"
#include "UI/Widget.h"

SpriteButton::SpriteButton(Object* owner, const std::string& backPath, const std::string& frontPath, const Color& backColor, const Color& frontColor)
	: Component(owner), backPath(backPath), frontPath(frontPath), backColor(backColor), frontColor(frontColor)
{
}

void SpriteButton::OnAwake()
{
	backTexture = ResourceManager::Instance().LoadTexture(backPath);
	frontTexture = ResourceManager::Instance().LoadTexture(frontPath);
}

void SpriteButton::OnUpdate()
{
	const auto* trigger = owner->GetComponent<EventTrigger>();
	const bool highlighted = trigger && trigger->IsInteractable() &&
		(trigger->IsSelected() || trigger->IsPointerInside() || trigger->IsPressed());
	const float targetAlpha = highlighted ? 1.0f : 0.0f;
	const float blend = std::clamp(Game::Time::unscaledDeltaTime * 14.0f, 0.0f, 1.0f);
	frontAlpha = std::lerp(frontAlpha, targetAlpha, blend);
}

void SpriteButton::OnRender(const RenderContext&)
{
	if (!owner->GetRectTransform()) return;
	auto* renderer = Game::Graphics::Instance().GetSpriteRenderer();

	// 背面
	renderer->Draw(SpriteShaderId::Basic, backTexture, {0.0f, 5.0f, 0.0f}, owner->GetRectTransform()->size,
		Vector2::Zero, Vector2(static_cast<float>(backTexture->GetWidth()), static_cast<float>(backTexture->GetHeight())),
		0.0f, Color(backColor.x, backColor.y, backColor.z, 1.0f));

	// 前面
	renderer->Draw(SpriteShaderId::Basic, frontTexture, {0.0f, 0.0f, 0.0f}, owner->GetRectTransform()->size + Vector2(0, 10.0f),
		Vector2::Zero, Vector2(static_cast<float>(frontTexture->GetWidth()), static_cast<float>(frontTexture->GetHeight())),
		0.0f, Color(frontColor.x, frontColor.y, frontColor.z, frontAlpha));
}

void SpriteButton::OnDrawGUI()
{
	ImGuiToolKit::ColorPicker3(u8"背面色", &backColor.x);
	ImGuiToolKit::ColorPicker3(u8"前面色", &frontColor.x);
}
