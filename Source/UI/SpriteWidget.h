#pragma once

#include "UI/Widget.h"
#include "Rendering/Renderer/SpriteRenderer.h"
#include <algorithm>
#include <filesystem>
#include <utility>

class SpriteWidget : public Widget
{
public:
	SpriteWidget(const std::string& name, std::filesystem::path spritePath,
		SpriteShaderId shaderId = SpriteShaderId::Basic,
		const Color& color = Color(1, 1, 1, 1),
		SpriteRenderParams renderParams = {});
	SpriteWidget(const std::string& name, std::shared_ptr<Texture> texture,
		SpriteShaderId shaderId = SpriteShaderId::Basic,
		const Color& color = Color(1, 1, 1, 1),
		SpriteRenderParams renderParams = {});
	void OnAwake() override;
	void DrawShadow(const RenderContext& rc, const Vector2& offset, const Color& color) override;

	Texture* GetTexture() const { return texture.get(); }
	void SetTexture(std::shared_ptr<Texture> texture) { this->texture = std::move(texture); }

	SpriteShaderId GetShaderId() const { return shaderId; }
	void SetShaderId(SpriteShaderId id) { shaderId = id; }
	const Color& GetColor() const { return color; }
	void SetColor(const Color& value) { color = value; }
	void SetHorizontalFill(float value) { horizontalFill = std::clamp(value, 0.0f, 1.0f); }
	float GetHorizontalFill() const { return horizontalFill; }
	void SetSourceRect(const Vector2& position, const Vector2& size)
	{
		sourcePosition = position;
		sourceSize = size;
		useSourceRect = true;
	}
	void ClearSourceRect() { useSourceRect = false; }
	SpriteRenderParams& GetRenderParams() { return renderParams; }
	const SpriteRenderParams& GetRenderParams() const { return renderParams; }

protected:
	void OnRender(const RenderContext& rc) override;
	void OnDrawGUI() override;

private:
	void DrawSprite(const Vector2& offset, const Color& drawColor,
		SpriteShaderId drawShader, const SpriteRenderParams* params);

	std::filesystem::path spritePath;
	std::shared_ptr<Texture> texture;
	SpriteShaderId shaderId;
	Color color;
	float horizontalFill = 1.0f;
	Vector2 sourcePosition = Vector2::Zero;
	Vector2 sourceSize = Vector2::Zero;
	bool useSourceRect = false;
	SpriteRenderParams renderParams;
};
