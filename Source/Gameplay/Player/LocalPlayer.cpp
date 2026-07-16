// LocalPlayer.cpp

#include "Gameplay/Player/LocalPlayer.h"

#include "Gameplay/Player/LocalPlayerController.h"

LocalPlayer::LocalPlayer()
    : Player()
{
    controller = AddComponent<LocalPlayerController>();
}
