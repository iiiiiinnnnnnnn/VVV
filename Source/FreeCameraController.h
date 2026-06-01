#pragma once
#include "CameraController.h"

class FreeCameraController : public CameraController
{
public:
	FreeCameraController(const Camera& camera);
	void SyncCameraToController(const Camera& camera) override;
	void SyncControllerToCamera(Camera& camera) override;
	void OnUpdate() override;
	void OnFocusLost() override;
};
