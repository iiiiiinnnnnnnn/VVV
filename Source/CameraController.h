#pragma once
#include "Camera.h"

class CameraController
{
public:
	// カメラからコントローラーへパラメータを同期する
	virtual void SyncCameraToController(const Camera& camera) = 0;

	// コントローラーからカメラへパラメータを同期する
	virtual void SyncControllerToCamera(Camera& camera) = 0;

	// 更新処理
	void Update(float elapsedTime) {

		// デバッグウインドウ操作中は処理しない
		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow))
		{
			return;
		}
		OnUpdate(elapsedTime);
	}

	virtual void OnUpdate(float elapsedTime) = 0;

protected:
	Vector3		eye;
	Vector3		focus;
	Vector3		up;
	Vector3		right;
	float		distance;

	float		angleX;
	float		angleY;
};
