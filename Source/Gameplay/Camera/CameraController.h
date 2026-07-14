#pragma once
#include "Core/Object/Component.h"
#include "Core/Object/Object.h"
#include "Gameplay/Camera/Camera.h"
#include "Application/Input/Input.h"

class CameraController : public Component
{
public:
	using Component::Component;

	virtual ~CameraController() = default;
	const char* GetDebugName() const override { return ICON_FA_VIDEO " CameraController"; }
	void SetInputEnabled(bool enabled)
	{
		inputEnabled = enabled;
		if (!inputEnabled) OnFocusLost();
	}

	// カメラからコントローラーへパラメータを同期する
	virtual void SyncCameraToController(const Camera& camera) = 0;

	// コントローラーからカメラへパラメータを同期する
	virtual void SyncControllerToCamera(Camera& camera) = 0;

	void OnAwake() override
	{
		SyncFromCamera();
	}

	void OnEnabled() override
	{
		SyncFromCamera();
	}

	void OnUpdate() override
	{
		if (!inputEnabled)
		{
			OnFocusLost();
			return;
		}

		if (Game::Input::IsFocusedWindow(!BlocksOnImGuiFocus())) {
			UpdateCamera();
		}
		else {
			OnFocusLost();
		}

		Camera* camera = owner->GetComponent<Camera>();
		if (!camera) return;

		SyncControllerToCamera(*camera);
	}

protected:
	void SyncFromCamera()
	{
		Camera* camera = owner->GetComponent<Camera>();
		if (!camera) return;

		SyncCameraToController(*camera);
	}

	virtual bool BlocksOnImGuiFocus() const { return true; }
	virtual void UpdateCamera() {}
	virtual void OnFocusLost() {}
	
	Vector3		eye;
	Vector3		focus;
	Vector3		up;
	Vector3		right;
	float		distance;

	float		angleX;
	float		angleY;
	bool inputEnabled = true;
};
