// MovieCameraController.cpp

#include "Gameplay/Camera/MovieCameraController.h"
#include "Application/Time/GameTime.h"

MovieCameraController::MovieCameraController(Object* owner)
	: CameraController(owner)
{
	//anim = std::make_unique<Animator>(this, true);
}

void MovieCameraController::SyncControllerToCamera(Camera& camera)
{

}

void MovieCameraController::UpdateCamera()
{

}

void MovieCameraController::OnFocusLost()
{

}

void MovieCameraController::OnDrawGUI()
{
	//anim->DrawGUI();
}
