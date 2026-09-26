// ThirdPersonCameraController.h
#pragma once

#include <algorithm>

#include "Gameplay/Camera/CameraController.h"
#include "Gameplay/Player/Player.h"

class Actor;

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
	float GetSensitivityScale() const { return mouseSensX / 0.005f; }
	void SetSensitivityScale(float scale)
	{
		scale = std::clamp(scale, 0.4f, 2.0f);
		mouseSensX = 0.005f * scale;
		mouseSensY = 0.003f * scale;
	}
	void RequestSkillFocus(float duration);
	void RequestBossDefeatFocus(Actor* target, float duration);
	void RequestAlignToPlayerForward();

protected:
	// プレイ中のカーソル解放はScene側で管理するため、ImGuiの残留フォーカスでは止めない
	bool BlocksOnImGuiFocus() const override { return false; }

private:
    Player* character;

    float armLength    = 11.0f;
    float heightOffset = 1.5f;
    float mouseSensX   = 0.005f;
    float mouseSensY   = 0.003f;
    float minArmLength = 0.5f;
    float maxArmLength = 50.0f;
    float followSpeed  = 4.5f;
	float sprintArmExtension = 2.0f;
	float sprintArmSpeed = 5.5f;
	float currentArmLength = 11.0f;
	float skillFocusTimer = 0.0f;
	float skillFocusDuration = 0.0f;
	float skillFocusArmLength = 4.5f;
	float skillFocusFovOffset = -6.0f;
	Actor* bossDefeatTarget = nullptr;
	float bossDefeatFocusTimer = 0.0f;
	float bossDefeatFocusDuration = 0.0f;

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
