#include "UI/DevelopMenuButtonWidget.h"

#include "UI/EventTrigger.h"

#include <utility>

DevelopMenuButtonWidget::DevelopMenuButtonWidget(const std::string& name, const std::string& label, float fontSize,
	const std::string& fontPath, std::function<void()> onActivated)
	: LabelButton(name, label, fontSize, fontPath)
{
	auto* trigger = AddComponent<EventTrigger>();
	trigger->SetNavigation(UINavigation::Automatic);
	trigger->SetWrapNavigation(true);
	trigger->SetInteractable(false);
	trigger->AddActivationListener(std::move(onActivated));
}