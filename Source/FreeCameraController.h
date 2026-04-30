#pragma once
#include "Camera.h"

class FreeCameraController
{
public:
	// カメラからコントローラーへパラメータを同期する
	void SyncCameraToController(const Camera& camera);

	// コントローラーからカメラへパラメータを同期する
	void SyncControllerToCamera(Camera& camera);

	// 更新処理
	void Update();

private:
	Vector3		eye;
	Vector3		focus;
	Vector3		up;
	Vector3		right;
	float		distance;

	float		angleX;
	float		angleY;
};
