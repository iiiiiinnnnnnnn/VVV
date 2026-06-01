// LocalPlayer.cpp

#include "LocalPlayer.h"
#include "Input.h"

float LocalPlayer::GetMoveX()
{
	if (!Game::Input::IsFocusedWindow()) return 0.0f;

	float i = Game::Input::Instance().GetGamePad().GetAxisLX();
	return i;
}

float LocalPlayer::GetMoveZ()
{
	if (!Game::Input::IsFocusedWindow()) return 0.0f;

	float i = Game::Input::Instance().GetGamePad().GetAxisLY();
	return i;
}

bool LocalPlayer::GetJump()
{
	if (!Game::Input::IsFocusedWindow()) return false;

	bool i = Game::Input::Instance().GetGamePad().GetButton() & GamePad::BTN_A;
	return i;
}

bool LocalPlayer::GetCrouch()
{
	if (!Game::Input::IsFocusedWindow()) return false;

	bool i = (bool)(Game::Input::Instance().GetGamePad().GetButton() & GamePad::BTN_B);
	return false;
}

bool LocalPlayer::GetReady()
{
	if (!Game::Input::IsFocusedWindow()) return false;

	bool i = Game::Input::Instance().GetMouse().GetButton() & Mouse::BTN_RIGHT;
	i |= (bool)(Game::Input::Instance().GetGamePad().GetButton() & GamePad::BTN_RIGHT_SHOULDER);
	return i;
}

bool LocalPlayer::GetShoot()
{
	if (!Game::Input::IsFocusedWindow()) return false;

	bool i = Game::Input::Instance().GetMouse().GetButton() & Mouse::BTN_LEFT;
	i |= (bool)(Game::Input::Instance().GetGamePad().GetButton() & GamePad::BTN_LEFT_SHOULDER);
	return i;
}

bool LocalPlayer::GetReload()
{
	if (!Game::Input::IsFocusedWindow()) return false;

	bool i = (bool)(Game::Input::Instance().GetGamePad().GetButton() & GamePad::BTN_Y);
	return false;
}
