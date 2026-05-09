// FpsCameraController.h

#pragma once

#include "CameraController.h"
#include "Player.h"

class FpsCameraController : public CameraController
{
public:
    FpsCameraController(std::shared_ptr<Player> player);
    void SyncCameraToController(const Camera& camera) override {}
    void SyncControllerToCamera(Camera& camera) override;
    void OnUpdate(float elapsedTime) override;

private:
    std::shared_ptr<Player> player = nullptr;
};