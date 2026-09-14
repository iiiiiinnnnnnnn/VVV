#pragma once

#include <functional>
#include <memory>
#include <string>

#include "UI/UIFont.h"
#include "UI/Widget.h"

class Texture;

class Button final : public Widget
{
public:
	Button(const std::string& name, const std::string& label);

	void SetOnClick(std::function<void()> value) { onClick = std::move(value); }
	void Activate();
	void SetSelected(bool value);
	bool IsSelected() const { return selected; }
	bool IsHovered() const { return hovered; }
	void SetFontSize(float value) { fontSize = value; }
	void SetLabel(const std::string& value) { label = value; }
	void SetSelectionTexture(const std::string& path);
	void SetSelectionSourceRect(const Vector2& position, const Vector2& size);

protected:
	void OnUpdate() override;
	void OnRender(const RenderContext& rc) override;

private:
	std::string label;
	std::function<void()> onClick;
	std::shared_ptr<Texture> selectionTexture;
	std::shared_ptr<Texture> whiteTexture;
	Vector2 selectionSourcePosition = Vector2::Zero;
	Vector2 selectionSourceSize = Vector2::Zero;
	bool useSelectionSource = false;
	bool selected = false;
	bool hovered = false;
	float fontSize = 42.0f;
	float focusBlend = 0.0f;
	float focusTime = 0.0f;
};
