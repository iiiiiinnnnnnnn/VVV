#pragma once

#include "Application/Time/Timer.h"

#include <memory>
#include <string>
#include <functional>

#include "UI/DissolveWidget.h"

class Texture;

class GaugeHUD : public DissolveWidget
{
public:
	GaugeHUD(const std::string& name, bool revealOnStart = true);

	// 値は0〜1。SetValueは補間し、SetValueImmediateは即時反映する。
	void SetValue(float ratio);
	void SetValueImmediate(float ratio);
	float GetValue() const { return targetValue; }
	float GetDisplayedValue() const { return displayedValue; }
	void SetValueSmoothingSpeed(float value) { smoothingSpeed = value; }

	void SetRevealDuration(float seconds) { revealDuration = std::max(seconds, 0.0f); }
	void SetFrameTexture(const std::string& path);
	void SetFillRect(const Vector2& position, const Vector2& size);
	// SetColorで通常色と低残量色を揃え、必要なら低残量色だけ個別に指定する。
	virtual void SetColor(const Color& value);
	const Color& GetColor() const { return normalColor; }
	void SetLowValueColor(const Color& value) { lowColor = value; }
	void SetFrameColor(const Color& value) { frameColor = value; }
	void SetBackgroundColor(const Color& value) { trackColor = value; }
	void SetDamageTrailColor(const Color& value) { damageTrailColor = value; }
	void SetDamageTrailEnabled(bool value) { damageTrailEnabled = value; }

	void SetOnValueChanged(std::function<void(float)> callback)
	{
		onValueChanged = std::move(callback);
	}
	bool IsDragging() const { return dragging; }

protected:
	void OnUpdate() override;
	void OnRender(const RenderContext& rc) override;
	void OnDrawGUI() override;

private:
	void DrawFillTiles(const Vector2& position, const Vector2& size, float ratio, const Color& color);
	std::shared_ptr<Texture> frameTexture;
	std::shared_ptr<Texture> fillTexture;
	Vector2 fillPosition = {0.1f, 0.3f};
	Vector2 fillSize = {0.8f, 0.4f};
	Color normalColor = Color(0.04f, 0.48f, 0.95f, 0.98f);
	Color lowColor = Color(1.0f, 0.035f, 0.02f, 0.98f);
	Color frameColor = Color(0.5f, 0.55f, 0.7f, 0.95f);
	Color trackColor = Color(0.015f, 0.035f, 0.055f, 0.88f);
	bool revealing = true;
	Game::Timer revealElapsed{0.0f};
	float revealDuration = 2.0f;
	float targetValue = 1.0f;
	float displayedValue = 1.0f;
	float smoothingSpeed = 5.5f;
	bool damageTrailEnabled = false;
	float damageTrailValue = 1.0f;
	float damageTrailDelay = 0.25f;
	Game::Timer damageTrailTimer{0.0f};
	float damageTrailSpeed = 0.65f;
	Color damageTrailColor = Color(0.48f, 0.52f, 0.57f, 0.98f);
	std::function<void(float)> onValueChanged;
	bool dragging = false;
};
