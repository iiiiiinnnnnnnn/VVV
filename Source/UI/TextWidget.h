#pragma once

#include <string>

#include "UI/UIFont.h"
#include "UI/Widget.h"

class TextWidget final : public Widget
{
public:
	TextWidget(const std::string& name, const std::string& text, float fontSize = 36.0f);

	void SetText(const std::string& value) { text = value; }
	const std::string& GetText() const { return text; }
	void SetFontSize(float value) { fontSize = value; }
	void SetColor(const Color& value) { color = value; }
	void SetAlignment(UITextAlignment value) { alignment = value; }

protected:
	void OnRender(const RenderContext& rc) override;

private:
	std::string text;
	float fontSize = 36.0f;
	Color color = Color(1, 1, 1, 1);
	UITextAlignment alignment = UITextAlignment::Left;
};
