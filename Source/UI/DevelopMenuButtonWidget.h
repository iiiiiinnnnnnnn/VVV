#pragma once

#include "UI/LabelButton.h"

#include <functional>

class DevelopMenuButtonWidget final : public LabelButton
{
public:
	DevelopMenuButtonWidget(const std::string& name, const std::string& label, float fontSize,
		const std::string& fontPath, std::function<void()> onActivated);
};