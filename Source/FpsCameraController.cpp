// FpsCameraController.cpp

#include "FpsCameraController.h"
#include <Input.h>

FpsCameraController::FpsCameraController(std::shared_ptr<Player> player)
{
    SetPlayer(player);
}

void FpsCameraController::SyncControllerToCamera(Camera& camera)
{
    // 目のノードのワールド位置を取得
    Model::Node* eyeNode = &player->GetModel()->GetNodes()[6];
    Vector3 eye(
        eyeNode->worldTransform._41,
        eyeNode->worldTransform._42,
        eyeNode->worldTransform._43
    );

    // カメラの向きから注視点を計算
    float sx = sinf(angleX);
    float cx = cosf(angleX);
    float sy = sinf(angleY);
    float cy = cosf(angleY);

    Vector3 front(-cx * sy, -sx, -cx * cy);
    Vector3 focus = eye + front;

    camera.SetLookAt(eye, focus, Vector3::Up);
}

void FpsCameraController::OnUpdate(float elapsedTime)
{
    Mouse& mouse = Input::Instance().GetMouse();
    mouse.SetCursorLock(true);
    mouse.SetCursorVisible(false);

    float moveX = mouse.GetAxisX() * 0.003f;
    float moveY = mouse.GetAxisY() * 0.003f;

    angleY += moveX;
    if (angleY > DirectX::XM_PI) angleY -= DirectX::XM_2PI;
    if (angleY < -DirectX::XM_PI) angleY += DirectX::XM_2PI;

    angleX += moveY;
    angleX = std::clamp(angleX,
        -DirectX::XMConvertToRadians(80.0f),
        DirectX::XMConvertToRadians(80.0f));

    player->transform.rotation = Quaternion::CreateFromAxisAngle(
        Vector3::UnitY, angleY);
}

void FpsCameraController::OnFocusLost()
{
    Mouse& mouse = Input::Instance().GetMouse();
    mouse.SetCursorLock(false);
    mouse.SetCursorVisible(true);
}
