// FpsCameraController.cpp

#include "FpsCameraController.h"
#include <Input.h>

FpsCameraController::FpsCameraController(std::shared_ptr<Player> player) : player(player)
{
}

void FpsCameraController::SyncControllerToCamera(Camera& camera)
{
    // 目のノードのワールド位置を取得
    Model::Node* eyeNode = &player->GetModel()->GetNodes()[10];
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

    float moveX = mouse.GetAxisX() * 0.003f;
    float moveY = mouse.GetAxisY() * 0.003f;

    angleY += moveX;
    if (angleY > DirectX::XM_PI) angleY -= DirectX::XM_2PI;
    if (angleY < -DirectX::XM_PI) angleY += DirectX::XM_2PI;

    angleX += moveY;
    angleX = std::clamp(angleX,
        -DirectX::XMConvertToRadians(80.0f),
        DirectX::XMConvertToRadians(80.0f));

    // プレイヤーをカメラのY角度に合わせて回転
    player->transform.rotation = Quaternion::CreateFromAxisAngle(
        Vector3::UnitY, angleY);

    // マウスを画面中央に固定
    HWND hWnd = GetActiveWindow();
    RECT rc;
    GetClientRect(hWnd, &rc);
    POINT center = { (rc.right - rc.left) / 2, (rc.bottom - rc.top) / 2 };
    ClientToScreen(hWnd, &center);
    SetCursorPos(center.x, center.y);
}
