#include "Gameplay/Player/LocalPlayer.h"

#include "Gameplay/Player/LocalPlayerInput.h"

LocalPlayer::LocalPlayer() = default;

void LocalPlayer::OnInit()
{
    Player::OnInit();
    AddComponent<LocalPlayerInput>();
}
