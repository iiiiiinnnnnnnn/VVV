// FreeCameraController.h
#pragma once
#include "Gameplay/Camera/CameraController.h"

class FreeCameraController : public CameraController
{
public:
	FreeCameraController(Object* owner);
	void SyncCameraToController(const Camera& camera) override;
	void SyncControllerToCamera(Camera& camera) override;
	void UpdateCamera() override;
	void OnFocusLost() override;

protected:
	bool BlocksOnImGuiFocus() const override { return false; }
};
