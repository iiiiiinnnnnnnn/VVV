// FpsCameraController.cpp

#include "FpsCameraController.h"
#include <Input.h>

FpsCameraController::FpsCameraController(std::shared_ptr<Player> character)
{
    SetPlayer(character);
}

void FpsCameraController::SyncControllerToCamera(Camera& camera)
{
    // 目のノードのワールド位置を取得
    Model::Node* eyeNode = &character->GetModel()->GetNodes()[6];
    Matrix world = Matrix::CreateTranslation(eyeOffset) * eyeNode->worldTransform;
    Vector3 eye(world._41, world._42, world._43);

    // カメラの向きから注視点を計算
    float sx = sinf(angleX);
    float cx = cosf(angleX);
    float sy = sinf(angleY);
    float cy = cosf(angleY);

    Vector3 front(cx * sy, -sx, cx * cy);
    Vector3 focus = eye + front;

    camera.SetLookAt(eye, focus, Vector3::Up);
}

void FpsCameraController::OnUpdate()
{
    Mouse& mouse = Game::Input::Instance().GetMouse();
    mouse.SetCursorLock(true);
    mouse.SetCursorVisible(false);

    float moveX = mouse.GetAxisX() * 0.003f;
    float moveY = mouse.GetAxisY() * 0.003f;

    // yaw
    angleY += moveX;
    if (angleY > DirectX::XM_PI) angleY -= DirectX::XM_2PI;
    if (angleY < -DirectX::XM_PI) angleY += DirectX::XM_2PI;

    // pitch
    angleX += moveY;

    angleX = std::clamp(angleX,
        -DirectX::XMConvertToRadians(80.0f),
        DirectX::XMConvertToRadians(80.0f));

    character->transform.rotation = Quaternion::CreateFromAxisAngle(
        Vector3::UnitY, angleY);

	character->SetFirstPerson(true);
    character->SetSpineAngleX(angleX);
}

void FpsCameraController::OnFocusLost()
{
    Mouse& mouse = Game::Input::Instance().GetMouse();
    mouse.SetCursorLock(false);
    mouse.SetCursorVisible(true);
}

void FpsCameraController::OnDrawGUI()
{
	ImGui::DragFloat3("Eye Offset", &eyeOffset.x, 0.01f);
}
