#include "UI/GaugeWidget.h"
#include <ImGuiToolKit/Widgets.h>
#include <ImGuiToolKit/Color.h>

#include <algorithm>

#include "Application/Time/GameTime.h"
#include <InputKit/InputKit.h>
#include "Rendering/Core/Graphics.h"
#include "Rendering/Component/SpriteRendererComponent.h"

GaugeWidget::GaugeWidget(const std::string& name, const std::string& maskPath, bool revealOnStart)
	: DissolveWidget(name, maskPath)
{
	SetDissolveAmount(revealOnStart ? 1.0f : 0.0f);
	SetAffectedByPostProcess(false);
}

void GaugeWidget::OnAwake()
{
	DissolveWidget::OnAwake();
	backgroundSprite = AddComponent<SpriteRendererComponent>("Resources/UI/guage_back.png");
	damageSprite = AddComponent<SpriteRendererComponent>("Resources/UI/guage_back.png");
	fillSprite = AddComponent<SpriteRendererComponent>("Resources/UI/guage_back.png");
	frameSprite = AddComponent<SpriteRendererComponent>("Resources/UI/guage_front.png");
	for (auto* sprite : {backgroundSprite, damageSprite, fillSprite})
		sprite->SetSourceRect({135.0f, 52.0f}, {1766.0f, 103.0f});
}

void GaugeWidget::SetValue(float ratio)
{
	const float value = std::clamp(ratio, 0.0f, 1.0f);
	if (damageTrailEnabled && value < targetValue)
	{
		damageTrailValue = std::max(damageTrailValue, displayedValue);
		damageTrailTimer.Reset(damageTrailDelay);
		displayedValue = std::min(displayedValue, value);
	}
	targetValue = value;
}

void GaugeWidget::SetValueImmediate(float ratio)
{
	SetValue(ratio);
	displayedValue = targetValue;
	damageTrailValue = targetValue;
	damageTrailTimer.Stop();
}

void GaugeWidget::SetFillRect(const Vector2& position, const Vector2& size)
{
	fillPosition = position;
	fillSize = size;
}

void GaugeWidget::OnUpdate()
{
	if (onValueChanged && targetDissolve < 1.0f && GetDissolveAmount() < 1.0f)
	{
		InputKit::Mouse& mouse = InputKit::InputSystem::Instance().GetMouse();
		const float fillLeft = rect.size.x * fillPosition.x;
		const float fillTop = rect.size.y * fillPosition.y;
		const float fillWidth = rect.size.x * fillSize.x;
		const float fillHeight = rect.size.y * fillSize.y;
		Vector2 localMouse = Vector2::Zero;
		const bool hasMouse = ScreenToLocal(Vector2(static_cast<float>(mouse.GetPositionX()),
			static_cast<float>(mouse.GetPositionY())), localMouse);
		const float mouseX = localMouse.x;
		const float mouseY = localMouse.y;
		if (hasMouse && (mouse.GetButtonDown() & InputKit::Mouse::BTN_LEFT) &&
			mouseX >= fillLeft && mouseX <= fillLeft + fillWidth &&
			mouseY >= fillTop && mouseY <= fillTop + fillHeight)
			dragging = true;
		if ((mouse.GetButton() & InputKit::Mouse::BTN_LEFT) == 0) dragging = false;
		if (dragging && hasMouse && fillWidth > 0.0f)
		{
			const float value = std::clamp((mouseX - fillLeft) / fillWidth, 0.0f, 1.0f);
			SetValue(value);
			onValueChanged(value);
		}
	}

	else dragging = false;

	const float dt = std::max(Game::Time::unscaledDeltaTime, 0.0f);
	// 途中で表示状態が反転しても、現在の量からそのまま目標へ進める
	const float amount = GetDissolveAmount();
	const float step = revealDuration > 0.0f ? dt / revealDuration : 1.0f;
	SetDissolveAmount(amount + std::clamp(targetDissolve - amount, -step, step));
	const float blend = std::clamp(smoothingSpeed * dt, 0.0f, 1.0f);
	displayedValue += (targetValue - displayedValue) * blend;
	if (std::abs(displayedValue - targetValue) < 0.0005f) displayedValue = targetValue;
	if (damageTrailEnabled)
	{
		const float trailDt = std::max(dt - damageTrailTimer.GetRemaining(), 0.0f);
		damageTrailTimer.Update(dt);
		damageTrailValue = std::max(displayedValue, damageTrailValue - damageTrailSpeed * trailDt);
	}
}

void GaugeWidget::OnDrawGUI()
{
	DissolveWidget::OnDrawGUI();

	ImGuiToolKit::ColorEdit4(u8"通常色", &normalColor.x);
	ImGuiToolKit::ColorEdit4(u8"背景色", &backgroundColor.x);
	ImGui::Separator();
	ImGuiToolKit::Checkbox(u8"ダメージ残像", &damageTrailEnabled);
	ImGuiToolKit::SliderFloat(u8"残像の待機時間", &damageTrailDelay, 0.0f, 1.0f);
	ImGuiToolKit::SliderFloat(u8"残像の減少速度", &damageTrailSpeed, 0.05f, 3.0f);
	ImGuiToolKit::ColorEdit4(u8"残像の色", &damageTrailColor.x);
}

void GaugeWidget::Render(const RenderContext& rc)
{
	if (!IsActive() || IsPendingDestroy() || GetDissolveAmount() >= 1.0f) return;
	const SpriteRenderParams& params = GetSpriteRenderParams();
	for (auto* sprite : {backgroundSprite, damageSprite, fillSprite})
	{
		sprite->SetRect(fillPosition, fillSize);
		sprite->SetRenderParams(params);
	}
	damageSprite->SetColor(damageTrailColor);
	damageSprite->SetFill(damageTrailEnabled ? damageTrailValue : 0.0f);
	auto toReddddd = Color::Lerp(
		normalColor, Color(1.0f, 0, 0, 1),
		Easing::Evaluate(1.0f - std::clamp(displayedValue * 1.5f, 0.0f, 1.0f), Easing::Type::OutCubic));
	fillSprite->SetColor(toReddddd);
	fillSprite->SetFill(displayedValue);
	frameSprite->SetRenderParams(params);
	backgroundSprite->SetColor(backgroundColor);
	Widget::Render(rc);
}
