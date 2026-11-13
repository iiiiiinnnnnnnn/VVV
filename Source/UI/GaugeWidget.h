#pragma once

#include "Application/Time/Timer.h"

#include <memory>
#include <string>
#include <functional>

#include "UI/DissolveWidget.h"

class SpriteRendererComponent;

class GaugeWidget : public DissolveWidget
{
public:
	void OnAwake() override;
	void Render(const RenderContext& rc) override;
	GaugeWidget(const std::string& name, const std::string& maskPath = "Resources/UI/dissolve_animation.png", bool revealOnStart = true);

	// 値は0〜1。SetValueは補間し、SetValueImmediateは即時反映する。
	void SetValue(float ratio);
	void SetValueImmediate(float ratio);
	float GetValue() const { return targetValue; }
	float GetDisplayedValue() const { return displayedValue; }
	void SetValueSmoothingSpeed(float value) { smoothingSpeed = value; }

	// 非表示中も更新を続け、ディゾルブだけを切り替える
	void SetVisibility(bool visible) { targetDissolve = visible ? 0.0f : 1.0f; }
	void SetRevealDuration(float seconds) { revealDuration = std::max(seconds, 0.0f); }
	void SetFillRect(const Vector2& position, const Vector2& size);
	void SetDamageTrailColor(const Color& value) { damageTrailColor = value; }
	void SetDamageTrailEnabled(bool value) { damageTrailEnabled = value; }

	void SetOnValueChanged(std::function<void(float)> callback)
	{
		onValueChanged = std::move(callback);
	}
	bool IsDragging() const { return dragging; }

protected:
	void OnUpdate() override;
	void OnDrawGUI() override;

	SpriteRendererComponent* frameSprite = nullptr;
	SpriteRendererComponent* backgroundSprite = nullptr;
	SpriteRendererComponent* damageSprite = nullptr;
	SpriteRendererComponent* fillSprite = nullptr;
	Vector2 fillPosition = {135.0f / 2036.0f, 52.0f / 221.0f};
	Vector2 fillSize = {1766.0f / 2036.0f, 103.0f / 221.0f};
	Color normalColor = Color(0.137f, 0.624f, 0.373f);
	Color backgroundColor = Color(0.071f, 0.0f, 0.0f);
	float targetDissolve = 0.0f;
	float revealDuration = 0.3f;
	float targetValue = 1.0f;
	float displayedValue = 1.0f;
	float smoothingSpeed = 5.5f;
	bool damageTrailEnabled = true;
	float damageTrailValue = 1.0f;
	float damageTrailDelay = 0.367f;
	Game::Timer damageTrailTimer{0.0f};
	float damageTrailSpeed = 0.411f;
	Color damageTrailColor = Color(1.0f, 0, 0, 0.1764f);
	std::function<void(float)> onValueChanged; // 変更通知
	bool dragging = false;
};
