// ThirdPersonCameraController.cpp
#include "Gameplay/Camera/ThirdPersonCameraController.h"
#include "Application/Input/Input.h"
#include "Application/Time/GameTime.h"
#include "Gameplay/Scene/CameraEffectController.h"
#include "Gameplay/Actor/Actor.h"

ThirdPersonCameraController::ThirdPersonCameraController(Object* owner, Player* character)
	: CameraController(owner)
{
    SetPlayer(character);
}

void ThirdPersonCameraController::RequestSkillFocus(float duration)
{
	if (duration <= 0.0f) return;
	skillFocusDuration = duration;
	skillFocusTimer = duration;
}

void ThirdPersonCameraController::RequestBossDefeatFocus(Actor* target, float duration)
{
	if (!target || duration <= 0.0f) return;
	bossDefeatTarget = target;
	bossDefeatFocusDuration = duration;
	bossDefeatFocusTimer = duration;
}

void ThirdPersonCameraController::RequestAlignToPlayerForward()
{
	const Vector3 forward = character->transform.forward;
	RequestYawAlignment(-atan2f(forward.x, forward.z), 10.0f);
}

void ThirdPersonCameraController::SyncControllerToCamera(Camera& camera)
{
    Vector3 playerPos = character->transform.position;
	Vector3 targetFocus = playerPos + Vector3(0, heightOffset, 0);
	const float baseArmLength = std::clamp(
		armLength + (character->IsSprinting() ? sprintArmExtension : 0.0f),
		minArmLength,
		maxArmLength);
	float skillFocusWeight = 0.0f;
	if (skillFocusTimer > 0.0f && skillFocusDuration > 0.0f)
	{
		const float elapsed = skillFocusDuration - skillFocusTimer;
		const float fadeIn = std::clamp(elapsed / 0.12f, 0.0f, 1.0f);
		const float fadeOut = std::clamp(skillFocusTimer / 0.28f, 0.0f, 1.0f);
		skillFocusWeight = std::min(fadeIn, fadeOut);
		skillFocusWeight =
			skillFocusWeight * skillFocusWeight * (3.0f - 2.0f * skillFocusWeight);
		skillFocusTimer = std::max(
			skillFocusTimer - Game::Time::unscaledDeltaTime, 0.0f);
	}
	const float targetArmLength = std::lerp(
		baseArmLength,
		std::clamp(skillFocusArmLength, minArmLength, maxArmLength),
		skillFocusWeight);

    // 最初のフレームの初期化
    if (!initialized)
    {
		currentArmLength = targetArmLength;
        // 角度から初期のオフセットを計算
        float sx = sinf(angleX); float cx = cosf(angleX);
        float sy = sinf(angleY); float cy = cosf(angleY);
        Vector3 offset(cx * sy * currentArmLength, -sx * currentArmLength,
			cx * -cy * currentArmLength);

        currentFocus = targetFocus;
        currentEye = targetFocus + offset;
        initialized = true;
    }

    // 1. まず注視点（Focus）を Lerp させる
    float t = 1.0f - expf(-followSpeed * Game::Time::deltaTime);
    currentFocus = Vector3::Lerp(currentFocus, targetFocus, t);
	const float armResponse = skillFocusWeight > 0.0f ? 18.0f : sprintArmSpeed;
	const float armT = 1.0f - expf(-armResponse * Game::Time::unscaledDeltaTime);
	currentArmLength += (targetArmLength - currentArmLength) * armT;

    // 2. 現在の角度から「理想のカメラ位置（めり込み前）」を計算する
    // ※ 角度の変更は OnUpdate で即座に反映されているため、
    //    もしカメラの回転自体もヌルッとさせたい場合は、angleX/Y 自体も Lerp してください。
    float sx = sinf(angleX);
    float cx = cosf(angleX);
    float sy = sinf(angleY);
    float cy = cosf(angleY);

    Vector3 offset(
        cx * sy * currentArmLength,
        -sx * currentArmLength,
        cx * -cy * currentArmLength
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

	float bossFocusWeight = 0.0f;
	float bossFocusFov = fovYDegrees;
	Vector3 cameraEye = finalEye;
	Vector3 cameraFocus = currentFocus;
	if (bossDefeatTarget && bossDefeatFocusTimer > 0.0f && bossDefeatFocusDuration > 0.0f)
	{
		const float elapsed = bossDefeatFocusDuration - bossDefeatFocusTimer;
		const float fadeIn = std::clamp(elapsed / 0.55f, 0.0f, 1.0f);
		const float fadeOut = std::clamp(bossDefeatFocusTimer / 0.9f, 0.0f, 1.0f);
		bossFocusWeight = std::min(fadeIn, fadeOut);
		bossFocusWeight = bossFocusWeight * bossFocusWeight *
			(3.0f - 2.0f * bossFocusWeight);

		const Vector3 bossPosition = bossDefeatTarget->transform.position;
		Vector3 viewDirection = character->transform.position - bossPosition;
		viewDirection.y = 0.0f;
		if (viewDirection.LengthSquared() < 0.001f) viewDirection = Vector3::Backward;
		viewDirection.Normalize();
		const float orbitAngle = std::min(elapsed * 0.16f, 0.75f);
		viewDirection = Vector3::TransformNormal(
			viewDirection, Matrix::CreateRotationY(orbitAngle));
		const Vector3 cinematicFocus = bossPosition + Vector3(0.0f, 2.6f, 0.0f);
		const Vector3 cinematicEye = cinematicFocus + viewDirection * 12.5f +
			Vector3(0.0f, 4.6f, 0.0f);
		cameraEye = Vector3::Lerp(finalEye, cinematicEye, bossFocusWeight);
		cameraFocus = Vector3::Lerp(currentFocus, cinematicFocus, bossFocusWeight);
		bossFocusFov = std::lerp(fovYDegrees, 41.0f, bossFocusWeight);

		bossDefeatFocusTimer = std::max(
			bossDefeatFocusTimer - Game::Time::unscaledDeltaTime, 0.0f);
		if (bossDefeatFocusTimer <= 0.0f) bossDefeatTarget = nullptr;
	}

	// 撃破カメラの移動中も地形へ潜り込まないよう、最終位置でも遮蔽判定する。
	if (bossFocusWeight > 0.0f)
	{
		Vector3 cinematicDirection = cameraEye - cameraFocus;
		const float cinematicDistance = cinematicDirection.Length();
		if (cinematicDistance > 0.001f)
		{
			cinematicDirection /= cinematicDistance;
			PxRaycastBuffer cinematicHit;
			PxQueryFilterData cinematicFilter;
			cinematicFilter.flags = PxQueryFlag::eSTATIC;
			if (scene->raycast(PxVec3(cameraFocus.x, cameraFocus.y, cameraFocus.z),
				PxVec3(cinematicDirection.x, cinematicDirection.y, cinematicDirection.z),
				cinematicDistance, cinematicHit, PxHitFlag::eDEFAULT, cinematicFilter))
			{
				const float distance = std::max(cinematicHit.block.distance - 0.1f, 0.1f);
				cameraEye = cameraFocus + cinematicDirection * distance;
			}
		}
	}

    // エフェクト系反映
    CameraEffectController::Update(camera, cameraEye, cameraFocus, Vector3::Up);

    camera.SetPerspectiveFov(
        DirectX::XMConvertToRadians(
			bossFocusFov + CameraEffectController::UpdateFovOffset() +
			skillFocusFovOffset * skillFocusWeight),
        aspectRatio,
        nearClip,
        farClip
    );

    // 次フレームの Lerp 用に現在の「理想位置」を保存しておく（必要に応じて）
    currentEye = finalEye;
}

void ThirdPersonCameraController::UpdateCamera()
{
    Mouse& mouse = Game::Input::Instance().GetMouse();
	GamePad& gamePad = Game::Input::Instance().GetGamePad();
    mouse.SetCursorLock(true);
    mouse.SetCursorVisible(false);
	if (bossDefeatTarget && bossDefeatFocusTimer > 0.0f)
	{
		character->SetFirstPerson(false);
		return;
	}

    float moveX = mouse.GetAxisX() * mouseSensX;
    float moveY = mouse.GetAxisY() * mouseSensY;
	constexpr float gamePadHorizontalSpeed = 2.8f;
	constexpr float gamePadVerticalSpeed = 2.2f;
	moveX += gamePad.GetAxisRX() * gamePadHorizontalSpeed * Game::Time::unscaledDeltaTime;
	moveY -= gamePad.GetAxisRY() * gamePadVerticalSpeed * Game::Time::unscaledDeltaTime;
	if (fabsf(moveX) > 0.0001f || fabsf(moveY) > 0.0001f)
		CancelYawAlignment();

    angleY -= moveX;
    if (angleY >  DirectX::XM_PI)  angleY -= DirectX::XM_2PI;
    if (angleY < -DirectX::XM_PI)  angleY += DirectX::XM_2PI;
	UpdateYawAlignment(Game::Time::unscaledDeltaTime);

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
	ImGui::DragFloat("Sprint Arm Extension", &sprintArmExtension, 0.05f, 0.0f, 10.0f);
	ImGui::DragFloat("Sprint Arm Speed", &sprintArmSpeed, 0.1f, 0.1f, 30.0f);
	ImGui::DragFloat("Skill Focus Arm Length", &skillFocusArmLength, 0.05f, minArmLength, maxArmLength);
	ImGui::DragFloat("Skill Focus FOV Offset", &skillFocusFovOffset, 0.25f, -30.0f, 0.0f);

    ImGui::Separator();

    ImGui::DragFloat("FOV Y", &fovYDegrees, 0.5f, 10.0f, 120.0f, "%.1f");
    ImGui::DragFloat("Near Clip", &nearClip, 0.01f, 0.01f, 10.0f);
    ImGui::DragFloat("Far Clip", &farClip, 10.0f, 10.0f, 10000.0f);

    fovYDegrees = std::clamp(fovYDegrees, 10.0f, 120.0f);
    nearClip = std::clamp(nearClip, 0.01f, 10.0f);
    farClip = std::clamp(farClip, nearClip + 1.0f, 10000.0f);
}
