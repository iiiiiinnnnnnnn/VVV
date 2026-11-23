#include "GamePadVib.h"
#include "Application/Time/GameTime.h"
#include "InputKit/InputKit.h"

GamePadVib::~GamePadVib()
{
	auto& gamepad = InputKit::InputSystem::Instance().GetGamePad();
	gamepad.SetVibration(0.0f, 0.0f);
}

void GamePadVib::Initialize()
{

}

void GamePadVib::Vibrate(float leftPower, float rightPower, float duration)
{
	float lp = std::clamp(leftPower, 0.0f, 1.0f);
	float rp = std::clamp(rightPower, 0.0f, 1.0f);
	if (lp > this->leftPower) this->leftPower = lp;
	if (rp > this->rightPower) this->rightPower = rp;
	this->duration += std::max(duration, 0.0f);
}

Vector2 GamePadVib::GetVibration()
{
	return Vector2(
		leftPower * std::clamp(duration, 0.0f, 1.0f),
		rightPower * std::clamp(duration, 0.0f, 1.0f));
}

void GamePadVib::Update()
{
	auto& gamepad = InputKit::InputSystem::Instance().GetGamePad();
	if (duration > 0.0f)
	{
		gamepad.SetVibration(leftPower, rightPower);
		duration -= Game::Time::deltaTime;
	}
	else
	{
		leftPower = 0.0f;
		rightPower = 0.0f;
		duration = 0.0f;
		gamepad.SetVibration(0.0f, 0.0f);
	}
}
