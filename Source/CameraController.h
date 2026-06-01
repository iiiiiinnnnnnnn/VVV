#pragma once
#include "Camera.h"
#include "Input.h"

class CameraController
{
public:
	virtual ~CameraController() = default;

	// カメラからコントローラーへパラメータを同期する
	virtual void SyncCameraToController(const Camera& camera) = 0;

	// コントローラーからカメラへパラメータを同期する
	virtual void SyncControllerToCamera(Camera& camera) = 0;

	// 更新処理
	void Update()
	{
		if (Game::Input::IsFocusedWindow()) {
			OnUpdate();
		}
		else {
			OnFocusLost();
		}
	}

	// フォーカスを失ったときの処理
	virtual void OnFocusLost() {}

	void DrawGUI() {
		OnDrawGUI();
	}

protected:
	virtual void OnUpdate() {}
	virtual void OnDrawGUI() {}
	
	Vector3		eye;
	Vector3		focus;
	Vector3		up;
	Vector3		right;
	float		distance;

	float		angleX;
	float		angleY;
};
