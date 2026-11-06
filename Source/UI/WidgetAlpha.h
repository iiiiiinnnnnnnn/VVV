#pragma once

#include "Core/Object/Component.h"
#include <algorithm>

class WidgetAlpha : public Component
{
public:
	WidgetAlpha(Object* owner, float alpha = 1.0f);
	void SetAlpha(float value) { alpha = std::clamp(value, 0.0f, 1.0f); }
	float GetAlpha() const { return alpha; }
	const char* GetDebugName() const override { return "WidgetAlpha"; }

protected:
	void OnDrawGUI() override;

private:
	float alpha = 1.0f;
};
