#include "UI/GaugeHUD.h"

#include <algorithm>
#include <cmath>

#include "Application/Time/GameTime.h"
#include "Application/Input/Input.h"
#include "Rendering/Core/Graphics.h"
#include "Rendering/Renderer/SpriteRenderer.h"
#include "Resource/ResourceManager.h"
#include "Resource/Texture.h"

GaugeHUD::GaugeHUD(const std::string& name)
	: Widget(name), fillTexture(std::make_shared<Texture>(Color(1, 1, 1, 1)))
{
	SetAffectedByPostProcess(false);
}

void GaugeHUD::SetFrameTexture(const std::string& path)
{
	frameTexture = ResourceManager::Instance().LoadTexture(path);
}

void GaugeHUD::SetEmblemTexture(const std::string& path)
{
	emblemTexture = ResourceManager::Instance().LoadTexture(path);
}

void GaugeHUD::SetTargetValue(float ratio)
{
	targetValue = std::clamp(ratio, 0.0f, 1.0f);
}

void GaugeHUD::SnapToTarget()
{
	displayedValue = targetValue;
}

void GaugeHUD::SetFillRect(const Vector2& position, const Vector2& size)
{
	fillPosition = position;
	fillSize = size;
}

void GaugeHUD::SetEmblemRect(const Vector2& position, const Vector2& size)
{
	emblemPosition = position;
	emblemSize = size;
}

void GaugeHUD::SetColors(const Color& normal, const Color& low, const Color& frame)
{
	normalColor = normal;
	lowColor = low;
	frameColor = frame;
}

void GaugeHUD::OnUpdate()
{
	if (onValueChanged)
	{
		Mouse& mouse = Game::Input::Instance().GetMouse();
		const Vector2 topLeft = rect.position - rect.size * rect.anchor;
		const float fillLeft = topLeft.x + rect.size.x * fillPosition.x;
		const float fillTop = topLeft.y + rect.size.y * fillPosition.y;
		const float fillWidth = rect.size.x * fillSize.x;
		const float fillHeight = rect.size.y * fillSize.y;
		const float mouseX = static_cast<float>(mouse.GetPositionX());
		const float mouseY = static_cast<float>(mouse.GetPositionY());
		if ((mouse.GetButtonDown() & Mouse::BTN_LEFT) &&
			mouseX >= fillLeft && mouseX <= fillLeft + fillWidth &&
			mouseY >= fillTop && mouseY <= fillTop + fillHeight)
			dragging = true;
		if ((mouse.GetButton() & Mouse::BTN_LEFT) == 0) dragging = false;
		if (dragging && fillWidth > 0.0f)
		{
			const float value = std::clamp((mouseX - fillLeft) / fillWidth, 0.0f, 1.0f);
			SetTargetValue(value);
			onValueChanged(value);
		}
	}

	const float dt = std::max(Game::Time::unscaledDeltaTime, 0.0f);
	const float blend = 1.0f - std::exp(-smoothingSpeed * dt);
	displayedValue += (targetValue - displayedValue) * blend;
	if (std::abs(displayedValue - targetValue) < 0.0005f) displayedValue = targetValue;
}

void GaugeHUD::OnRender(const RenderContext&)
{
	SpriteRenderer* renderer = Game::Graphics::Instance().GetSpriteRenderer();
	const Vector2 topLeft = rect.position - rect.size * rect.anchor;
	if (frameTexture)
		renderer->Draw(SpriteShaderId::Basic, frameTexture, {topLeft.x, topLeft.y, 0.0f}, rect.size,
			Vector2::Zero, {static_cast<float>(frameTexture->GetWidth()),
				static_cast<float>(frameTexture->GetHeight())}, rect.angle, frameColor);
	const Vector2 fillTopLeft = topLeft + Vector2(rect.size.x * fillPosition.x, rect.size.y * fillPosition.y);
	const Vector2 maximumFill = {rect.size.x * fillSize.x, rect.size.y * fillSize.y};
	const float ratio = std::clamp(displayedValue, 0.0f, 1.0f);
	if (fillTexture)
		renderer->Draw(SpriteShaderId::Basic, fillTexture,
			{fillTopLeft.x, fillTopLeft.y, 0.0f}, maximumFill,
			Vector2::Zero, Vector2(1, 1), 0.0f, trackColor);
	if (fillTexture && ratio > 0.0f)
		renderer->Draw(SpriteShaderId::Basic, fillTexture,
			{fillTopLeft.x, fillTopLeft.y, 0.0f}, {maximumFill.x * ratio, maximumFill.y},
			Vector2::Zero, Vector2(1, 1), 0.0f, ratio < 0.3f ? lowColor : normalColor);
	if (emblemTexture && emblemSize.x > 0.0f && emblemSize.y > 0.0f)
	{
		const Vector2 emblemTopLeft = topLeft + Vector2(
			rect.size.x * emblemPosition.x, rect.size.y * emblemPosition.y);
		renderer->Draw(SpriteShaderId::Basic, emblemTexture,
			{emblemTopLeft.x, emblemTopLeft.y, 0.0f},
			{rect.size.x * emblemSize.x, rect.size.y * emblemSize.y}, Vector2::Zero,
			{static_cast<float>(emblemTexture->GetWidth()), static_cast<float>(emblemTexture->GetHeight())},
			0.0f, frameColor);
	}
}
