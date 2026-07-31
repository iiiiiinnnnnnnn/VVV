// LocalPlayerController.h

#pragma once

#include "Gameplay/Player/PlayerController.h"

class LocalPlayerController : public PlayerController
{
public:
    LocalPlayerController(Object* owner) : PlayerController(owner) {}

    InputContext Poll() override;
    const char* GetDebugName() const override { return ICON_FA_GAMEPAD " LocalPlayerController"; }

private:
    bool quickStepKeyHeld = false;
    unsigned int quickStepDirectionMask = 0;
    float quickStepBufferTimer = 0.0f;
};
