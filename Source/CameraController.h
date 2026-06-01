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
	virtual void SyncControllerToCamera(Camera& camera, float elapsedTime) = 0;

	// 更新処理
	void Update(float elapsedTime)
	{
		if (Input::IsFocusedWindow()) {
			OnUpdate(elapsedTime);
		}
		else {
			OnFocusLost();
		}
	}

	// フォーカスを失ったときの処理
	virtual void OnFocusLost() {}

	void DrawGUI(float elapsedTime) {
		OnDrawGUI(elapsedTime);
	}

protected:
	virtual void OnUpdate(float elapsedTime) {}
	virtual void OnDrawGUI(float elapsedTime) {}
	
	Vector3		eye;
	Vector3		focus;
	Vector3		up;
	Vector3		right;
	float		distance;

	float		angleX;
	float		angleY;
};
