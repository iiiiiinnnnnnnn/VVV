// ThirdPersonCameraController.cpp

#include "ThirdPersonCameraController.h"
#include "Input.h"
#include "GameTime.h"
#include "CameraEffectController.h"

ThirdPersonCameraController::ThirdPersonCameraController(Player* character)
{
    SetPlayer(character);
}

void ThirdPersonCameraController::SyncControllerToCamera(Camera& camera)
{
    Vector3 playerPos = character->transform.position;
    Vector3 targetFocus = playerPos + Vector3(0, heightOffset, 0);

    // 最初のフレームの初期化
    if (!initialized)
    {
        // 角度から初期のオフセットを計算
        float sx = sinf(angleX); float cx = cosf(angleX);
        float sy = sinf(angleY); float cy = cosf(angleY);
        Vector3 offset(cx * sy * armLength, -sx * armLength, cx * -cy * armLength);

        currentFocus = targetFocus;
        currentEye = targetFocus + offset;
        initialized = true;
    }

    // 1. まず注視点（Focus）を Lerp させる
    float t = 1.0f - expf(-followSpeed * Game::Time::deltaTime);
    currentFocus = Vector3::Lerp(currentFocus, targetFocus, t);

    // 2. 現在の角度から「理想のカメラ位置（めり込み前）」を計算する
    // ※ 角度の変更は OnUpdate で即座に反映されているため、
    //    もしカメラの回転自体もヌルッとさせたい場合は、angleX/Y 自体も Lerp してください。
    float sx = sinf(angleX);
    float cx = cosf(angleX);
    float sy = sinf(angleY);
    float cy = cosf(angleY);

    Vector3 offset(
        cx * sy * armLength,
        -sx * armLength,
        cx * -cy * armLength
    );

    // Lerp 済みの currentFocus を基準に、理想のカメラ位置を決める
    Vector3 idealEye = currentFocus + offset;

    // 3. 最後にレイキャスト（めり込み防止）を行う
    Vector3 dir = idealEye - currentFocus;
    float maxDist = dir.Length();
    dir.Normalize();

    PxScene* scene = PhysicsManager::Instance().GetSceneContext().GetScene();
    PxVec3 origin(currentFocus.x, currentFocus.y, currentFocus.z);
    PxVec3 unitDir(dir.x, dir.y, dir.z);

    PxRaycastBuffer hit;
    PxQueryFilterData filterData;
    filterData.flags = PxQueryFlag::eSTATIC;

    // 最終的な表示位置を決定する変数
    Vector3 finalEye = idealEye;

    if (scene->raycast(origin, unitDir, maxDist, hit, PxHitFlag::eDEFAULT, filterData))
    {
        float hitDist = hit.block.distance - 0.1f;
        if (hitDist < 0.1f) hitDist = 0.1f;
        finalEye = currentFocus + dir * hitDist;
    }

    // エフェクト系反映
    CameraEffectController::Update(camera, finalEye, currentFocus, Vector3::Up);

    camera.SetPerspectiveFov(
        DirectX::XMConvertToRadians(fovYDegrees),
        aspectRatio,
        nearClip,
        farClip
    );

    // 次フレームの Lerp 用に現在の「理想位置」を保存しておく（必要に応じて）
    currentEye = finalEye;
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

    ImGui::Separator();

    ImGui::DragFloat("FOV Y", &fovYDegrees, 0.5f, 10.0f, 120.0f, "%.1f");
    ImGui::DragFloat("Near Clip", &nearClip, 0.01f, 0.01f, 10.0f);
    ImGui::DragFloat("Far Clip", &farClip, 10.0f, 10.0f, 10000.0f);

    fovYDegrees = std::clamp(fovYDegrees, 10.0f, 120.0f);
    nearClip = std::clamp(nearClip, 0.01f, 10.0f);
    farClip = std::clamp(farClip, nearClip + 1.0f, 10000.0f);
}

