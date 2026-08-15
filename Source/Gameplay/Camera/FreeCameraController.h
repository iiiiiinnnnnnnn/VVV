#pragma once
#include "Gameplay/Camera/CameraController.h"

class FreeCameraController : public CameraController
{
public:
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
};
