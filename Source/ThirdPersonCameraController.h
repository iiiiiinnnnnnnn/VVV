// ThirdPersonCameraController.h

#pragma once

#include "CameraController.h"
#include "Player.h"

class ThirdPersonCameraController : public CameraController
{
public:
    ThirdPersonCameraController(std::shared_ptr<Player> chara);

    void SyncCameraToController(const Camera& camera) override {}
    void SyncControllerToCamera(Camera& camera) override;
    void OnUpdate() override;
    void OnFocusLost() override;
    void OnDrawGUI() override;

    void SetPlayer(std::shared_ptr<Player> character) { this->character = character; }

    float GetCameraYaw() const { return -angleY; }

private:
    std::shared_ptr<Player> character = nullptr;

    float armLength    = 5.0f;
    float heightOffset = 1.5f;
    float mouseSensX   = 0.005f;
    float mouseSensY   = 0.003f;
    float minArmLength = 0.5f;
    float maxArmLength = 5.0f;
    float followSpeed  = 5.0f;

    // 現在のカメラ位置・注視点（Lerpの現在値）
    Vector3 currentEye   = Vector3::Zero;
    Vector3 currentFocus = Vector3::Zero;
    bool    initialized  = false;
};
