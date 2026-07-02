// FpsCameraController.h

#pragma once
#include <memory>

#include "CameraController.h"
#include "Player.h"

class FpsCameraController : public CameraController
{
public:
    FpsCameraController(std::shared_ptr<Player> chara);
    void SyncCameraToController(const Camera& camera) override {}
    void SyncControllerToCamera(Camera& camera) override;
    void OnUpdate() override;
    void OnFocusLost() override;
    void OnDrawGUI() override;

	void SetPlayer(std::shared_ptr<Player> character) { this->character = character; }

private:
    std::shared_ptr<Player> character = nullptr;
    Vector3 eyeOffset = {0, 0.09f, 0};
};
