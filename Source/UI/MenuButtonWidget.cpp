#include "UI/MenuButtonWidget.h"

#include "UI/EventTrigger.h"
#include "UI/SpriteButton.h"

#include <utility>

#include "Audio/SoundSystem.h"

MenuButtonWidget::MenuButtonWidget(const std::string& name, const std::string& label, float fontSize,
	const std::string& fontPath, std::function<void()> onActivated)
	: LabelButton(name, label, fontSize, fontPath)
{
	auto spritebtn = AddComponent<SpriteButton>(
		"Resources/UI/btn_back.png", "Resources/UI/btn_front.png");
	spritebtn->SetBackColor({0, 0, 0});
	spritebtn->SetFrontColor({1, 1, 1});
	spritebtn->SetFrontOffset({0, 18.0f});
	spritebtn->SetBlendSpeed(28.0f);

	SetFontSize(35.0f);
	SetOffset({20.0f, 4.0f});

	auto* trigger = AddComponent<EventTrigger>();
	trigger->SetNavigation(UINavigation::Automatic);
	trigger->SetWrapNavigation(true);
	trigger->SetInteractable(false);
	trigger->AddActivationListener(std::move(onActivated));
}

void MenuButtonWidget::PlayClickSound()
{
	SoundSystem::Instance().Play("SE/UI/ButtonClick");
}
