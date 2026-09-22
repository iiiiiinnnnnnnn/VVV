#pragma once

#include <memory>
#include <string>
#include <functional>

#include "UI/Widget.h"

class Texture;

class GaugeHUD : public Widget
{
public:
	GaugeHUD(const std::string& name);

	void SetFrameTexture(const std::string& path);
	void SetEmblemTexture(const std::string& path);
	void SetTargetValue(float ratio);
	void SnapToTarget();
	float GetDisplayedValue() const { return displayedValue; }
	void SetSmoothingSpeed(float value) { smoothingSpeed = value; }
	void SetFillRect(const Vector2& position, const Vector2& size);
	void SetEmblemRect(const Vector2& position, const Vector2& size);
	void SetColors(const Color& normal, const Color& low, const Color& frame);
	void SetTrackColor(const Color& value) { trackColor = value; }
	void SetInteractive(std::function<void(float)> callback)
	{
		onValueChanged = std::move(callback);
	}
	bool IsDragging() const { return dragging; }

protected:
	void OnUpdate() override;
	void OnRender(const RenderContext& rc) override;

private:
	std::shared_ptr<Texture> frameTexture;
	std::shared_ptr<Texture> emblemTexture;
	std::shared_ptr<Texture> fillTexture;
	Vector2 fillPosition = {0.1f, 0.3f};
	Vector2 fillSize = {0.8f, 0.4f};
	Vector2 emblemPosition = Vector2::Zero;
	Vector2 emblemSize = Vector2::Zero;
	Color normalColor = Color(0.82f, 0.12f, 0.09f, 0.94f);
	Color lowColor = Color(1.0f, 0.035f, 0.02f, 0.98f);
	Color frameColor = Color(0.5f, 0.55f, 0.7f, 0.95f);
	Color trackColor = Color(0.015f, 0.035f, 0.055f, 0.88f);
	float targetValue = 1.0f;
	float displayedValue = 1.0f;
	float smoothingSpeed = 5.5f;
	std::function<void(float)> onValueChanged;
	bool dragging = false;
};
