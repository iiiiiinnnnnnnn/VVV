// LocalPlayer.cpp

#include "LocalPlayer.h"
#include "Input.h"

float LocalPlayer::GetMoveX()
{
	if (Input::IsFocusedWindow()) {
		return Input::Instance().GetGamePad().GetAxisLX();
	}
	else {
		return 0.0f;
	}
}

float LocalPlayer::GetMoveZ()
{
	if (Input::IsFocusedWindow()) {
		return Input::Instance().GetGamePad().GetAxisLY();
	}
	else {
		return 0.0f;
	}
}

bool LocalPlayer::GetJump()
{
	return false;
}

bool LocalPlayer::GetAttack()
{
	return false;
}
