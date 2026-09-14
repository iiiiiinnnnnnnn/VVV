#include "UI/TextWidget.h"

TextWidget::TextWidget(const std::string& name, const std::string& text, float fontSize)
	: Widget(name), text(text), fontSize(fontSize)
{
	SetAffectedByPostProcess(false);
}

void TextWidget::OnRender(const RenderContext&)
{
	const Vector2 topLeft = rect.position - rect.size * rect.anchor;
	UIFont::Default().DrawText(text, topLeft, rect.size, fontSize, color, alignment);
}
