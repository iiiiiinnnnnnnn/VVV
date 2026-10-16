#include "Gameplay/Player/LocalPlayer.h"

#include "Gameplay/Player/LocalPlayerInput.h"

LocalPlayer::LocalPlayer()
    : Player()
{
    AddComponent<LocalPlayerInput>();
}
