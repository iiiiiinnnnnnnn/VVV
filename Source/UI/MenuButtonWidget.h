#pragma once

#include "UI/LabelButton.h"

#include <functional>

class MenuButtonWidget final : public LabelButton
{
public:
	MenuButtonWidget(const std::string& name, const std::string& label, float fontSize,
		const std::string& fontPath, std::function<void()> onActivated);
};