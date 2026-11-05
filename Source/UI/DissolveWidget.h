#pragma once
#include <algorithm>

#include "UI/Widget.h"
#include "Rendering/Core/SpriteRenderParams.h"

class DissolveWidget : public Widget
{
public:
	void OnAwake() override;
	DissolveWidget(const std::string& name = "Dissolve Widget",
		const std::string& maskPath = "Resources/UI/dissolve_animation.png");
	void SetTexture(const std::string& path);
	void SetDissolveMask(const std::string& path);
	void SetDissolveAmount(float value) { renderParams.dissolve.amount = std::clamp(value, 0.0f, 1.0f); }
	float GetDissolveAmount() const { return renderParams.dissolve.amount; }
	void SetColor(const Color& value) { spriteColor = value; }
	void SetDissolveMaskScale(float value) { renderParams.dissolve.maskScale = std::clamp(value, 0.01f, 10.0f); }
	void SetDissolveSoftness(float value) { renderParams.dissolve.softness = std::clamp(value, 0.0f, 0.5f); }

protected:
	void OnRender(const RenderContext& rc) override;
	void OnDrawGUI() override;
	void DrawSprite(const std::shared_ptr<Texture>& texture, const Vector2& position,
		const Vector2& size, const Vector2& sourceSize, float angle, const Color& color);

private:
	Color spriteColor = Color(1, 1, 1, 1);
	std::shared_ptr<Texture> texture;
	SpriteRenderParams renderParams;

private:
	std::string maskPath;
	std::string texturePath;
};
