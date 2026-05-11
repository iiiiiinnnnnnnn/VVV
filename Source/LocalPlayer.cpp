// LocalPlayer.cpp

#include "LocalPlayer.h"
#include "Input.h"

float LocalPlayer::GetMoveX()
{
	if (Input::IsFocusedWindow()) {
		float axisLX = Input::Instance().GetGamePad().GetAxisLX();
		return axisLX;
	}
	else {
		return 0.0f;
	}
}

float LocalPlayer::GetMoveZ()
{
	if (Input::IsFocusedWindow()) {
		float axisLY = Input::Instance().GetGamePad().GetAxisLY();
		return axisLY;
	}
	else {
		return 0.0f;
	}
}

bool LocalPlayer::GetJump()
{
	return false;
}

bool LocalPlayer::GetShoot()
{
	return false;
}

bool LocalPlayer::GetReload()
{
	return false;
}
