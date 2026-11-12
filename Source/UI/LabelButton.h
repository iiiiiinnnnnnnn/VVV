#pragma once

#include "UI/TextWidget.h"

class LabelButton : public TextWidget
{
public:
	LabelButton(const std::string& name, const std::string& label, float fontSize = 42.0f,
		const std::string& fontPath = "");

	void SetNormalColor(const Color& value) { normalColor = value; }
	void SetHighlightColor(const Color& value) { highlightColor = value; }
	void SetOffset(const Vector2& value) { SetTextOffset(value); }

protected:
	void OnRender(const RenderContext& rc) override;
	void OnDrawGUI() override;

private:
	Color normalColor = Color(0.86f, 0.91f, 0.95f, 1.0f);
	Color highlightColor = Color(0.82f, 0.97f, 1.0f, 1.0f);
};
