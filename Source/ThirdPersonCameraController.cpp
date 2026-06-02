// ThirdPersonCameraController.cpp

#include "ThirdPersonCameraController.h"
#include "Input.h"

ThirdPersonCameraController::ThirdPersonCameraController(std::shared_ptr<Player> character)
{
    SetPlayer(character);
}

void ThirdPersonCameraController::SyncControllerToCamera(Camera& camera)
{
    // プレイヤーのワールド位置を注視点の基準にする
    Vector3 playerPos = character->transform.position;
    Vector3 focusPos  = playerPos + Vector3(0, heightOffset, 0);

    // angleX(pitch) / angleY(yaw) からカメラ位置を計算
    float sx = sinf(angleX);
    float cx = cosf(angleX);
    float sy = sinf(angleY);
    float cy = cosf(angleY);

    // 球面座標でカメラオフセットを求める（後ろ側 = -Z 方向を基準）
    Vector3 offset(
        cx * sy * armLength,
        -sx      * armLength,
        cx * cy * armLength
    );

    Vector3 eyePos = focusPos + offset;

    camera.SetLookAt(eyePos, focusPos, Vector3::Up);
}

void ThirdPersonCameraController::OnUpdate()
{
    Mouse& mouse = Game::Input::Instance().GetMouse();
    mouse.SetCursorLock(true);
    mouse.SetCursorVisible(false);

    float moveX = mouse.GetAxisX() * mouseSensX;
    float moveY = mouse.GetAxisY() * mouseSensY;

    // Yaw（水平回転）
    angleY += moveX;
    if (angleY >  DirectX::XM_PI)  angleY -= DirectX::XM_2PI;
    if (angleY < -DirectX::XM_PI)  angleY += DirectX::XM_2PI;

    // Pitch（垂直回転）：見下ろし?見上げを制限
    angleX += moveY;
    angleX = std::clamp(angleX,
                        DirectX::XMConvertToRadians(-60.0f),   // 見上げ
                        DirectX::XMConvertToRadians( 70.0f));  // 見下ろし

    // プレイヤーはカメラのYaw方向には回転させない（移動入力で向きを決める）
    // 必要であれば character->transform.rotation をここで設定してもよい
    character->SetFirstPerson(false);
}

void ThirdPersonCameraController::OnFocusLost()
{
    Mouse& mouse = Game::Input::Instance().GetMouse();
    mouse.SetCursorLock(false);
    mouse.SetCursorVisible(true);
}

void ThirdPersonCameraController::OnDrawGUI()
{
    ImGui::DragFloat("Arm Length",    &armLength,    0.05f, minArmLength, maxArmLength);
    ImGui::DragFloat("Height Offset", &heightOffset, 0.05f, 0.0f, 3.0f);
    ImGui::DragFloat("Sens X",        &mouseSensX,   0.0005f, 0.0001f, 0.01f);
    ImGui::DragFloat("Sens Y",        &mouseSensY,   0.0005f, 0.0001f, 0.01f);
}
