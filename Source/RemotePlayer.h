// RemotePlayer.h

#pragma once

#include "PlayerController.h"

class RemotePlayer : public PlayerController {
public:
    float GetMoveX() override { return receivedMoveX; }
    float GetMoveZ() override { return receivedMoveZ; }
    bool GetJump() override { return receivedJump; }
    bool GetShoot() override { return receivedReload; }
    bool GetReload() override { return receivedReload; }

    void ReceiveInput(float moveX, float moveZ, bool jump, bool shoot, bool reload) {
        receivedMoveX = moveX;
        receivedMoveZ = moveZ;
        receivedJump = jump;
        receivedShoot = shoot;
        receivedReload = reload;
    }

    void ReceivePositionCorrection(Vector3 pos) { serverPosition = pos; }
    Vector3 GetServerPosition() const { return serverPosition; }

private:
    float receivedMoveX = 0.0f;
    float receivedMoveZ = 0.0f;
    bool receivedJump = false;
    bool receivedShoot = false;
    bool receivedReload = false;
    Vector3 serverPosition;
};
