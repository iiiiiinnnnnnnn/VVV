// TextWidget.h
#pragma once

#include <string>

#include "UI/UIFont.h"
#include "UI/Widget.h"

class TextWidget : public Widget
{
public:
	void OnAwake() override;
	TextWidget(const std::string& name, const std::string& text, float fontSize = 36.0f,
		const std::string& fontPath = "");
	bool SetFont(const std::string& path);
	const std::string& GetFontPath() const { return fontPath; }

	void SetText(const std::string& value) { text = value; }
	const std::string& GetText() const { return text; }
	void SetTextOffset(const Vector2& value) { textOffset = value; }
	void SetFontSize(float value) { fontSize = value; }
	void SetColor(const Color& value) { color = value; }
	void SetAlignment(UITextAlignment value) { alignment = value; }
	void DrawShadow(const RenderContext& rc, const Vector2& offset,
		const Color& color) override;

protected:
	void OnRender(const RenderContext& rc) override;
	void OnDrawGUI() override;

	void DrawFont(const Vector2& topLeft, const Color& drawColor);

	std::string text;
	Vector2 textOffset = Vector2::Zero;
	std::shared_ptr<UIFont> font;
	std::string fontPath;
	float fontSize = 36.0f;
	Color color = Color(1, 1, 1, 1);
	UITextAlignment alignment = UITextAlignment::Left;
};
