#include <imgui.h>
#include "FreeCameraController.h"
#include "GameTime.h"

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
	const float lerpSpeed = 5.0f;
    camera.SetLookAt(
        Vector3::Lerp(camera.GetEye(), eye, Game::Time::unscaledDeltaTime * lerpSpeed),
        Vector3::Lerp(camera.GetFocus(), focus, Game::Time::unscaledDeltaTime * lerpSpeed),
        Vector3::Lerp(camera.GetUp(), up, Game::Time::unscaledDeltaTime * lerpSpeed)
	);
}

void FreeCameraController::OnUpdate()
{
    if (!Game::Input::Instance().IsFocusedWindow())
        return;

    ImGuiIO& io = ImGui::GetIO();

    // マウスカーソルの移動量
    float moveX = io.MouseDelta.x * 0.01f;
    float moveY = io.MouseDelta.y * 0.01f;

    // 現在の入力状態を取得
    bool isShiftPressed = io.KeyShift;
    bool isAltPressed = io.KeyAlt;
    bool isRightMouseDown = io.MouseDown[ImGuiMouseButton_Right];
    bool isLeftMouseDown = io.MouseDown[ImGuiMouseButton_Left];

    // 左クリックだけでは見渡さない。Alt + 左クリックのときだけオービット操作にする
    bool isAltLeftMouseDown = isAltPressed && isLeftMouseDown;

    if (isAltLeftMouseDown || isRightMouseDown)
    {
        // Y軸回転
        angleY += moveX * 0.5f;

        // X軸回転
        angleX += moveY * 0.5f;

        // ジンバルロック防止
        const float pitchLimit = DirectX::XM_PIDIV2 - 0.01f;
        if (angleX > pitchLimit)  angleX = pitchLimit;
        if (angleX < -pitchLimit) angleX = -pitchLimit;

        // angleY の値を -PI 〜 +PI の範囲に丸める
        if (angleY > DirectX::XM_PI)       angleY -= DirectX::XM_2PI;
        else if (angleY < -DirectX::XM_PI) angleY += DirectX::XM_2PI;
    }

    // 回転を反映した各方向ベクトルを計算
    float sx = ::sinf(angleX);
    float cx = ::cosf(angleX);
    float sy = ::sinf(angleY);
    float cy = ::cosf(angleY);

    Vector3 Front = Vector3(-cx * sy, -sx, -cx * cy);
    Front.Normalize();

    Vector3 Right = Vector3(cy, 0, -sy);
    Right.Normalize();

    Vector3 Up = Right.Cross(Front);

    if (isRightMouseDown)
    {
        // 右ドラッグ中のみ、WASDキーによるFPS移動を許可
        float speed = 5.0f;
        if (isShiftPressed)
        {
            speed *= 3.0f;
        }

        Vector3 moveDir = Vector3::Zero;

        if (ImGui::IsKeyDown(ImGuiKey_W)) moveDir += Front;
        if (ImGui::IsKeyDown(ImGuiKey_S)) moveDir -= Front;
        if (ImGui::IsKeyDown(ImGuiKey_A)) moveDir += Right;
        if (ImGui::IsKeyDown(ImGuiKey_D)) moveDir -= Right;
        if (ImGui::IsKeyDown(ImGuiKey_E)) moveDir += Up;
        if (ImGui::IsKeyDown(ImGuiKey_Q)) moveDir -= Up;

        if (moveDir.LengthSquared() > 0.0f)
        {
            moveDir.Normalize();
            eye += moveDir * speed * Game::Time::unscaledDeltaTime;
        }

        // FPSモードは eye を基準に focus を決める
        focus = eye + (Front * distance);
    }
    else if (isAltLeftMouseDown)
    {
        // Alt + 左ドラッグ時だけオービット
        eye = focus - (Front * distance);
    }
    else
    {
        // ホイールズーム
        if (io.MouseWheel != 0.0f)
        {
            distance -= io.MouseWheel * distance * 0.1f;
            if (distance < 0.1f) distance = 0.1f;
        }

        // 中ボタンドラッグでパン
        if (io.MouseDown[ImGuiMouseButton_Middle])
        {
            float panSpeed = distance * 0.2f;
            Vector3 pan = Right * moveX * panSpeed + Up * moveY * panSpeed;
            eye += pan;
            focus += pan;
        }

        // 通常時は注視点をベースに位置を確定
        eye = focus - (Front * distance);
    }

    // 最終的な行列計算とパラメータ同期
    Matrix View = Matrix::CreateLookAt(eye, focus, Up);
    Matrix World = DirectX::XMMatrixTranspose(View);

    right = Vector3::TransformNormal(Vector3(1, 0, 0), World);
    up = Vector3::TransformNormal(Vector3(0, 1, 0), World);
}

void FreeCameraController::OnFocusLost()
{
    // ウィンドウのフォーカスが外れた際、ImGuiの入力状態（押しっぱなし判定など）が
    // 残ってカメラが暴走するのを防ぐため、内部の入力を一度リセットする
    ImGui::GetIO().ClearEventsQueue();
}