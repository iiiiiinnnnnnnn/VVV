// FpsCameraController.h

#pragma once

#include "CameraController.h"
#include "Character.h"

class FpsCameraController : public CameraController
{
public:
    FpsCameraController(std::shared_ptr<Character> chara);
    void SyncCameraToController(const Camera& camera) override {}
    void SyncControllerToCamera(Camera& camera, float elapsedTime) override;
    void OnUpdate(float elapsedTime) override;
    void OnFocusLost() override;
    void OnDrawGUI(float elapsedTime) override;

	void SetPlayer(std::shared_ptr<Character> character) { this->character = character; }

private:
    std::shared_ptr<Character> character = nullptr;
    Vector3 eyeOffset = {0, 0.09f, 0};
};
