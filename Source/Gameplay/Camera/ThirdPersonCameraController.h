// ThirdPersonCameraController.h

#pragma once

#include "Gameplay/Camera/CameraController.h"
#include "Gameplay/Player/Player.h"

class ThirdPersonCameraController : public CameraController
{
public:
    ThirdPersonCameraController(Object* owner, Player* chara);

    void SyncCameraToController(const Camera& camera) override {}
    void SyncControllerToCamera(Camera& camera) override;
    void UpdateCamera() override;
    void OnFocusLost() override;
    void OnDrawGUI() override;

    void SetPlayer(Player* character) { this->character = character; }

    float GetCameraYaw() const { return -angleY; }

private:
    Player* character;

    float armLength    = 7.0f;
    float heightOffset = 1.5f;
    float mouseSensX   = 0.005f;
    float mouseSensY   = 0.003f;
    float minArmLength = 0.5f;
    float maxArmLength = 50.0f;
    float followSpeed  = 5.0f;

	const float FOV_DEFAULT = 50.0f;
    float fovYDegrees = 50.0f;
    float aspectRatio = 1280.0f / 720.0f;
    float nearClip = 0.1f;
    float farClip = 1000.0f;

    bool initialized = false;

    // 現在のカメラ位置・注視点（Lerpの現在値）
    Vector3 currentEye   = Vector3::Zero;
    Vector3 currentFocus = Vector3::Zero;
};
