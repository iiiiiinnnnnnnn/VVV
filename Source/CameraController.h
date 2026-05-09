#pragma once
#include "Camera.h"

class CameraController
{
public:
	virtual ~CameraController() = default;

	// カメラからコントローラーへパラメータを同期する
	virtual void SyncCameraToController(const Camera& camera) = 0;

	// コントローラーからカメラへパラメータを同期する
	virtual void SyncControllerToCamera(Camera& camera) = 0;

	// 更新処理
	void Update(float elapsedTime)
	{
		// デバッグウインドウ操作中は処理しない
		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow))
		{
			OnFocusLost();
			return;
		}

		// ウィンドウが最前面でない場合は処理しない
		HWND hWnd = GetActiveWindow();
		if (hWnd == nullptr)
		{
			OnFocusLost();
			return;
		}

		OnUpdate(elapsedTime);
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
