// MovieCameraController.h

#pragma once
#include <memory>

#include "Gameplay/Camera/CameraController.h"
#include "Animation/Animator.h"

class MovieCameraController : public CameraController
{
public:
    MovieCameraController(Object* owner);

    void SyncCameraToController(const Camera& camera) override {}
    void SyncControllerToCamera(Camera& camera) override;
    void UpdateCamera() override;
    void OnFocusLost() override;
    void OnDrawGUI() override;

private:
    Vector3 currentEye   = Vector3::Zero;
    Vector3 currentFocus = Vector3::Zero;

    std::unique_ptr<Animator> anim;
};
