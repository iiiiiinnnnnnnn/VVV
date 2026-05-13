// LocalPlayer.cpp

#include "LocalPlayer.h"
#include "Input.h"

float LocalPlayer::GetMoveX()
{
	if (!Input::IsFocusedWindow()) return 0.0f;

	float axisLX = Input::Instance().GetGamePad().GetAxisLX();
	return axisLX;
}

float LocalPlayer::GetMoveZ()
{
	if (!Input::IsFocusedWindow()) return 0.0f;

	float axisLY = Input::Instance().GetGamePad().GetAxisLY();
	return axisLY;
}

bool LocalPlayer::GetJump()
{
	if (!Input::IsFocusedWindow()) return false;

	bool jump = Input::Instance().GetGamePad().GetButton() & GamePad::BTN_A;
	return jump;
}

bool LocalPlayer::GetCrouch()
{
	return false;
}

bool LocalPlayer::GetReady()
{
	if (!Input::IsFocusedWindow()) return false;

	bool ready = Input::Instance().GetMouse().GetButton() & Mouse::BTN_RIGHT;
	ready |= (Input::Instance().GetGamePad().GetButton() & GamePad::BTN_RIGHT_SHOULDER);
	return ready;
}

bool LocalPlayer::GetShoot()
{
	return false;
}

bool LocalPlayer::GetReload()
{
	return false;
}
