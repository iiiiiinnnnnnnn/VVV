#pragma once

#include "UI/WidgetGroup.h"
#include "Gameplay/Player/LocalPlayerInput.h"
#include <utility>

class XboxControllerWidget : public WidgetGroup
{
public:
	explicit XboxControllerWidget(
		const std::string& name, std::weak_ptr<Object> inputOwner = {}, const Vector2& position = Vector2::Zero, const Vector2& scale = Vector2::One);
	void SetInputOwner(std::weak_ptr<Object> value) { inputOwner = std::move(value); }

protected:
	void OnAwake() override;
	void OnUpdate() override;
	void OnRender(const RenderContext& rc) override;
	void OnDrawGUI() override;

private:
	using Control = LocalPlayerInput::Control;
	void AddControl(const std::vector<std::shared_ptr<Widget>>& widgets,
		Control control, Control secondaryControl = Control::None);

	struct ControlHighlight
	{
		std::vector<std::shared_ptr<Widget>> widgets;
		Control control;
		Control secondaryControl;
		float offTimer = 0.4f; // 消灯タイマー(これがないとチカチカする)
	};
	std::vector<ControlHighlight> highlights;
	std::weak_ptr<Object> inputOwner;
	Vector2 vibrationPower = Vector2::Zero;
	Vector2 vibrationAmplitude = Vector2::Zero;
	Vector2 vibrationOffset = Vector2::Zero;
	float vibrationDuration = 0.5f;
	float vibrationRemaining = 0.0f;
	float previousVibrationDuration = 0.0f;
};
