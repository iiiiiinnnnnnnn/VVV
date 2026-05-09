#include <imgui.h>
#include "FreeCameraController.h"

FreeCameraController::FreeCameraController(const Camera& camera)
{
	SyncCameraToController(camera);
}

// カメラからコントローラーへパラメータを同期する
void FreeCameraController::SyncCameraToController(const Camera& camera)
{
	eye = camera.GetEye();
	focus = camera.GetFocus();
	up = camera.GetUp();
	right = camera.GetRight();

	// 視点から注視点までの距離を算出
	distance = Vector3::Distance(eye, focus);

	// 回転角度を算出
	const Vector3& front = camera.GetFront();
	angleX = ::asinf(-front.y);
	if (up.y < 0)
	{
		if (front.y > 0)
		{
			angleX = -DirectX::XM_PI - angleX;
		}
		else
		{
			angleX = DirectX::XM_PI - angleX;
		}
		angleY = ::atan2f(front.x, front.z);
	}
	else
	{
		angleY = ::atan2f(-front.x, -front.z);
	}

}

// コントローラーからカメラへパラメータを同期する
void FreeCameraController::SyncControllerToCamera(Camera& camera)
{
	camera.SetLookAt(eye, focus, up);
}

// 更新処理
void FreeCameraController::OnUpdate(float elapsedTime)
{
	// IMGUIのマウス入力値を使ってカメラ操作する
	ImGuiIO io = ImGui::GetIO();

	// マウスカーソルの移動量を求める
	float moveX = io.MouseDelta.x * 0.02f;
	float moveY = io.MouseDelta.y * 0.02f;

	// マウス左ボタン押下中
	if (io.MouseDown[ImGuiMouseButton_Right])
	{
		// Y軸回転
		angleY += moveX * 0.5f;
		if (angleY > DirectX::XM_PI)
		{
			angleY -= DirectX::XM_2PI;
		}
		else if (angleY < -DirectX::XM_PI)
		{
			angleY += DirectX::XM_2PI;
		}
		// X軸回転
		angleX += moveY * 0.5f;
		if (angleX > DirectX::XM_PI)
		{
			angleX -= DirectX::XM_2PI;
		}
		else if (angleX < -DirectX::XM_PI)
		{
			angleX += DirectX::XM_2PI;
		}
	}
	// マウス中ボタン押下中
	else if (io.MouseDown[ImGuiMouseButton_Middle])
	{
		// 平行移動
		float s = distance * 0.035f;
		float x = moveX * s;
		float y = moveY * s;

		focus.x += right.x * x;
		focus.y += right.y * x;
		focus.z += right.z * x;

		focus.x += up.x * y;
		focus.y += up.y * y;
		focus.z += up.z * y;
	}
	// マウス右ボタン押下中
	else if (io.MouseDown[ImGuiMouseButton_Left] && io.MouseDown[ImGuiMouseButton_Right])
	{
		// ズーム
		distance += (-moveY - moveX) * distance * 0.1f;
	}
	// マウスホイール
	else if (io.MouseWheel != 0)
	{
		// ズーム
		distance -= io.MouseWheel * distance * 0.1f;
	}

	float sx = ::sinf(angleX);
	float cx = ::cosf(angleX);
	float sy = ::sinf(angleY);
	float cy = ::cosf(angleY);

	// カメラの方向を算出
	Vector3 Front = Vector3(-cx * sy, -sx, -cx * cy);
	Vector3 Right = Vector3(cy, 0, -sy);
	Vector3 Up = Right.Cross(Front);

	// カメラの視点＆注視点を算出
	Vector3 Focus = focus;
	Vector3 Eye = Focus - (Front * distance);

	// ビュー行列からワールド行列を算出
	Matrix View = Matrix::CreateLookAt(Eye, Focus, Up);
	Matrix World = DirectX::XMMatrixTranspose(View);

	// ワールド行列から方向を算出
	Right = Vector3::TransformNormal(Vector3(1, 0, 0), World);
	Up = Vector3::TransformNormal(Vector3(0, 1, 0), World);

	// 結果を格納
	eye = Eye;
	up = Up;
	right = Right;
}
