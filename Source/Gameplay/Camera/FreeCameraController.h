#pragma once
#include "Gameplay/Camera/CameraController.h"

class FreeCameraController : public CameraController
{
public:
	static constexpr float MouseRotationSensitivity = 0.005f;
	static constexpr float CameraFollowSpeed = 5.0f;
	static constexpr float VerticalFovDegrees = 45.0f;

	FreeCameraController(Object* owner);
	void SetMoveSpeed(float speed) { moveSpeed = (std::max)(speed, 0.0f); }
	float GetMoveSpeed() const { return moveSpeed; }
	void FocusOn(const Vector3& target);
	void SyncCameraToController(const Camera& camera) override;
	void SyncControllerToCamera(Camera& camera) override;
	void UpdateCamera() override;
	void OnFocusLost() override;

protected:
	bool BlocksOnImGuiFocus() const override { return false; }

private:
	float moveSpeed = 5.0f;
	float moveSpeedOverlayTimer = 0.0f;
};
