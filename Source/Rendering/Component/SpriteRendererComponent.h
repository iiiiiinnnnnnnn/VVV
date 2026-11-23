#pragma once
#include "Core/Object/Component.h"
#include "Core/Foundation/Common.h"
#include "Rendering/Core/SpriteRenderParams.h"

class Texture;

// Widgetのローカル矩形を使って画像を描画する
class SpriteRendererComponent : public Component
{
public:
	SpriteRendererComponent(Object* owner, const std::string& path);
	void OnAwake() override;
	void OnRender(const RenderContext& rc) override;
	const char* GetDebugName() const override { return ICON_FA_IMAGE " スプライト描画"; }
	void SetTexture(const std::string& path);
	void SetRect(const Vector2& position, const Vector2& size) { localPosition = position; localSize = size; }
	void SetSourceRect(const Vector2& position, const Vector2& size) { sourcePosition = position; sourceSize = size; }
	void SetFill(float value) { fill = std::clamp(value, 0.0f, 1.0f); }
	void SetColor(const Color& value) { color = value; }
	void SetRenderParams(const SpriteRenderParams& value) { renderParams = value; }

private:
	std::string path;
	std::shared_ptr<Texture> texture;
	Vector2 localPosition = Vector2::Zero;
	Vector2 localSize = Vector2::One;
	Vector2 sourcePosition = Vector2::Zero;
	Vector2 sourceSize = Vector2::Zero;
	Color color = Color(1, 1, 1, 1);
	float fill = 1.0f;
	SpriteRenderParams renderParams;
};
