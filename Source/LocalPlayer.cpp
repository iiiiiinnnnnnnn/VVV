// LocalPlayer.cpp

#include "LocalPlayer.h"
#include "Input.h"

float LocalPlayer::GetMoveX()
{
	if (!Input::IsFocusedWindow()) return 0.0f;

	float i = Input::Instance().GetGamePad().GetAxisLX();
	return i;
}

float LocalPlayer::GetMoveZ()
{
	if (!Input::IsFocusedWindow()) return 0.0f;

	float i = Input::Instance().GetGamePad().GetAxisLY();
	return i;
}

bool LocalPlayer::GetJump()
{
	if (!Input::IsFocusedWindow()) return false;

	bool i = Input::Instance().GetGamePad().GetButton() & GamePad::BTN_A;
	return i;
}

bool LocalPlayer::GetCrouch()
{
	if (!Input::IsFocusedWindow()) return false;

	bool i = (bool)(Input::Instance().GetGamePad().GetButton() & GamePad::BTN_B);
	return false;
}

bool LocalPlayer::GetReady()
{
	if (!Input::IsFocusedWindow()) return false;

	bool i = Input::Instance().GetMouse().GetButton() & Mouse::BTN_RIGHT;
	i |= (bool)(Input::Instance().GetGamePad().GetButton() & GamePad::BTN_RIGHT_SHOULDER);
	return i;
}

bool LocalPlayer::GetShoot()
{
	if (!Input::IsFocusedWindow()) return false;

	bool i = Input::Instance().GetMouse().GetButton() & Mouse::BTN_LEFT;
	i |= (bool)(Input::Instance().GetGamePad().GetButton() & GamePad::BTN_LEFT_SHOULDER);
	return i;
}

bool LocalPlayer::GetReload()
{
	if (!Input::IsFocusedWindow()) return false;

	bool i = (bool)(Input::Instance().GetGamePad().GetButton() & GamePad::BTN_Y);
	return false;
}
