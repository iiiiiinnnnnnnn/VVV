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
	: DissolveWidget(name, ""), fillTexture(ResourceManager::Instance().LoadTexture("Resources/UI/gauge.png"))
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
	const float value = std::clamp(ratio, 0.0f, 1.0f);
	if (damageTrailEnabled && value < targetValue)
	{
		damageTrailValue = std::max(damageTrailValue, displayedValue);
		damageTrailTimer = damageTrailDelay;
		displayedValue = std::min(displayedValue, value);
	}
	targetValue = value;
}

void GaugeHUD::SnapToTarget()
{
	displayedValue = targetValue;
	damageTrailValue = targetValue;
	damageTrailTimer = 0.0f;
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
	if (damageTrailEnabled)
	{
		const float trailDt = std::max(dt - damageTrailTimer, 0.0f);
		damageTrailTimer = std::max(damageTrailTimer - dt, 0.0f);
		damageTrailValue = std::max(displayedValue, damageTrailValue - damageTrailSpeed * trailDt);
	}
}

void GaugeHUD::OnDrawGUI()
{
	DissolveWidget::OnDrawGUI();
	ImGui::Checkbox((const char*)u8"ダメージ残像", &damageTrailEnabled);
	ImGui::SliderFloat((const char*)u8"残像の待機時間", &damageTrailDelay, 0.0f, 1.0f);
	ImGui::SliderFloat((const char*)u8"残像の減少速度", &damageTrailSpeed, 0.05f, 3.0f);
	ImGui::ColorEdit4((const char*)u8"残像の色", &damageTrailColor.x);
}

void GaugeHUD::DrawFillTiles(const Vector2& position, const Vector2& size, float ratio, const Color& color)
{
	if (!fillTexture || size.x <= 0.0f || size.y <= 0.0f || ratio <= 0.0f) return;
	const Vector2 sourceSize = {
		static_cast<float>(fillTexture->GetWidth()), static_cast<float>(fillTexture->GetHeight())};
	if (sourceSize.x <= 0.0f || sourceSize.y <= 0.0f) return;
	const float tileWidth = size.y * sourceSize.x / sourceSize.y;
	const float width = size.x * std::clamp(ratio, 0.0f, 1.0f);
	for (float x = 0.0f; x < width; x += tileWidth)
	{
		const float visibleWidth = std::min(tileWidth, width - x);
		DrawSprite(fillTexture, position + Vector2(x, 0.0f), {visibleWidth, size.y},
			{sourceSize.x * visibleWidth / tileWidth, sourceSize.y}, 0.0f, color);
	}
}

void GaugeHUD::OnRender(const RenderContext&)
{
	const Vector2 topLeft = rect.position - rect.size * rect.anchor;
	if (frameTexture)
		DrawSprite(frameTexture, topLeft, rect.size,
			{static_cast<float>(frameTexture->GetWidth()),
				static_cast<float>(frameTexture->GetHeight())}, rect.angle, frameColor);
	const Vector2 fillTopLeft = topLeft + Vector2(rect.size.x * fillPosition.x, rect.size.y * fillPosition.y);
	const Vector2 maximumFill = {rect.size.x * fillSize.x, rect.size.y * fillSize.y};
	const float ratio = std::clamp(displayedValue, 0.0f, 1.0f);
	DrawFillTiles(fillTopLeft, maximumFill, 1.0f, trackColor);
	if (damageTrailEnabled && damageTrailValue > ratio)
		DrawFillTiles(fillTopLeft, maximumFill, damageTrailValue, damageTrailColor);
	DrawFillTiles(fillTopLeft, maximumFill, ratio, ratio < 0.3f ? lowColor : normalColor);
	if (emblemTexture && emblemSize.x > 0.0f && emblemSize.y > 0.0f)
	{
		const Vector2 emblemTopLeft = topLeft + Vector2(
			rect.size.x * emblemPosition.x, rect.size.y * emblemPosition.y);
		DrawSprite(emblemTexture, emblemTopLeft,
			{rect.size.x * emblemSize.x, rect.size.y * emblemSize.y},
			{static_cast<float>(emblemTexture->GetWidth()), static_cast<float>(emblemTexture->GetHeight())},
			0.0f, frameColor);
	}
}
