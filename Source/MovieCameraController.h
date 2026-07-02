// MovieCameraController.h

#pragma once
#include <memory>

#include "CameraController.h"
#include "Animator.h"

class MovieCameraController : public CameraController
{
public:
    MovieCameraController();

    void SyncCameraToController(const Camera& camera) override {}
    void SyncControllerToCamera(Camera& camera) override;
    void OnUpdate() override;
    void OnFocusLost() override;
    void OnDrawGUI() override;

private:
    Vector3 currentEye   = Vector3::Zero;
    Vector3 currentFocus = Vector3::Zero;

    std::unique_ptr<Animator> anim;
};
