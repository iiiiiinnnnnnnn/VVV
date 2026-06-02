// ThirdPersonCameraController.cpp

#include "ThirdPersonCameraController.h"
#include "Input.h"
#include "GameTime.h"

ThirdPersonCameraController::ThirdPersonCameraController(std::shared_ptr<Player> character)
{
    SetPlayer(character);
}

void ThirdPersonCameraController::SyncControllerToCamera(Camera& camera)
{
    Vector3 playerPos = character->transform.position;
    Vector3 targetFocus = playerPos + Vector3(0, heightOffset, 0);

    float sx = sinf(angleX);
    float cx = cosf(angleX);
    float sy = sinf(angleY);
    float cy = cosf(angleY);

    Vector3 offset(
        cx * sy * armLength,
        -sx      * armLength,
        cx * -cy * armLength
    );
    Vector3 targetEye = targetFocus + offset;

    // ‰‰ñ‚ÍuŽž‚ÉƒZƒbƒg
    if (!initialized)
    {
        currentEye   = targetEye;
        currentFocus = targetFocus;
        initialized  = true;
    }

    // Žw”Lerp‚ÅŠŠ‚ç‚©‚É’Ç]
    float t = 1.0f - expf(-followSpeed * Game::Time::deltaTime);
    currentEye   = Vector3::Lerp(currentEye,   targetEye,   t);
    currentFocus = Vector3::Lerp(currentFocus, targetFocus, t);

    camera.SetLookAt(currentEye, currentFocus, Vector3::Up);
}

void ThirdPersonCameraController::OnUpdate()
{
    Mouse& mouse = Game::Input::Instance().GetMouse();
    mouse.SetCursorLock(true);
    mouse.SetCursorVisible(false);

    float moveX = mouse.GetAxisX() * mouseSensX;
    float moveY = mouse.GetAxisY() * mouseSensY;

    angleY -= moveX;
    if (angleY >  DirectX::XM_PI)  angleY -= DirectX::XM_2PI;
    if (angleY < -DirectX::XM_PI)  angleY += DirectX::XM_2PI;

    angleX -= moveY;
    angleX = std::clamp(angleX,
                        DirectX::XMConvertToRadians(-60.0f),
                        DirectX::XMConvertToRadians( 70.0f));

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
    ImGui::DragFloat("Follow Speed",  &followSpeed,  0.5f,   1.0f, 30.0f);
}
