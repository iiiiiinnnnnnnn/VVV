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
        -sx * armLength,
        cx * -cy * armLength
    );

    Vector3 targetEye = targetFocus + offset;

    // ---- カメラめり込み防止 ----
    Vector3 dir = targetEye - targetFocus;
    float maxDist = dir.Length();
    dir.Normalize();

    PxScene* scene = PhysicsManager::Instance().GetSceneContext().GetScene();
    PxVec3 origin(targetFocus.x, targetFocus.y, targetFocus.z);
    PxVec3 unitDir(dir.x, dir.y, dir.z);

    PxRaycastBuffer hit;
    PxQueryFilterData filterData;
    filterData.flags = PxQueryFlag::eSTATIC;  // 静的コライダーのみ（壁・床）

    if (scene->raycast(origin, unitDir, maxDist, hit, PxHitFlag::eDEFAULT, filterData))
    {
        // 当たった位置より少し手前にカメラを引く
        float hitDist = hit.block.distance - 0.1f;
        if (hitDist < 0.1f) hitDist = 0.1f;
        targetEye = targetFocus + dir * hitDist;
    }
    // ----------------------------

    if (!initialized)
    {
        currentEye = targetEye;
        currentFocus = targetFocus;
        initialized = true;
    }

    float t = 1.0f - expf(-followSpeed * Game::Time::deltaTime);
    currentEye = Vector3::Lerp(currentEye, targetEye, t);
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
