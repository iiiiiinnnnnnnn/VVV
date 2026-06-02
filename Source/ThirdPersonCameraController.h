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

private:
    std::shared_ptr<Player> character = nullptr;

    // カメラ設定
    float armLength     = 2.5f;   // プレイヤーからカメラまでの距離
    float heightOffset  = 1.2f;   // 注視点の高さオフセット（プレイヤー原点から）
    float mouseSensX    = 0.003f;
    float mouseSensY    = 0.003f;

    // 障害物検出用の最小・最大距離
    float minArmLength  = 0.5f;
    float maxArmLength  = 5.0f;
};
