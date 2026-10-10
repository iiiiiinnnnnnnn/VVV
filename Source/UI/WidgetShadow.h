#pragma once

#include "Core/Object/Component.h"
#include "Core/Foundation/Common.h"

// 影に対応したWidgetへ影描画を追加する
class WidgetShadow : public Component
{
public:
	WidgetShadow(Object* owner, const Vector2& offset = Vector2(3.0f, 3.0f),
		const Color& color = Color(0.0f, 0.0f, 0.0f, 0.7f));

	const char* GetDebugName() const override { return "WidgetShadow"; }

	void SetOffset(const Vector2& value) { offset = value; }
	void SetColor(const Color& value) { color = value; }
	const Vector2& GetOffset() const { return offset; }
	const Color& GetColor() const { return color; }

protected:
	void OnRender(const RenderContext& rc) override;
	void OnDrawGUI() override;

private:
	Vector2 offset;
	Color color;
};
