#include "UI/LabelButton.h"
#include <ImGuiToolKit/Color.h>
#include <ImGuiToolKit/Drag.h>

#include "UI/EventTrigger.h"

LabelButton::LabelButton(const std::string& name, const std::string& label, float fontSize,
	const std::string& fontPath)
	: TextWidget(name, label, fontSize, fontPath)
{
}

void LabelButton::OnRender(const RenderContext& rc)
{
	const auto* trigger = GetComponent<EventTrigger>();
	const bool highlighted = trigger && trigger->IsInteractable() &&
		(trigger->IsSelected() || trigger->IsPointerInside());
	SetColor(highlighted ? highlightColor : normalColor);
	TextWidget::OnRender(rc);
}

void LabelButton::OnDrawGUI()
{
	TextWidget::OnDrawGUI();
	ImGuiToolKit::ColorEdit4(u8"通常色", &normalColor.x);
	ImGuiToolKit::ColorEdit4(u8"選択色", &highlightColor.x);
	ImGuiToolKit::DragPosFloat2(u8"オフセット", &textOffset.x);
}
