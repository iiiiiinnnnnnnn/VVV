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
void FreeCameraController::SyncControllerToCamera(Camera& camera, float elapsedTime)
{
    #if 1 // lerp
	const float lerpSpeed = 5.0f;
    camera.SetLookAt(
        Vector3::Lerp(camera.GetEye(), eye, elapsedTime * lerpSpeed),
        Vector3::Lerp(camera.GetFocus(), focus, elapsedTime * lerpSpeed),
        Vector3::Lerp(camera.GetUp(), up, elapsedTime * lerpSpeed)
	);
    #else
    // 直接反映
    camera.SetLookAt(eye, focus, up);
	#endif
}

void FreeCameraController::OnUpdate(float elapsedTime)
{
    if (!Input::Instance().IsFocusedWindow())
        return;

    ImGuiIO& io = ImGui::GetIO();

    // マウスカーソルの移動量（感度は必要に応じて調整してください）
    float moveX = io.MouseDelta.x * 0.01f;
    float moveY = io.MouseDelta.y * 0.01f;

    // 現在の入力状態を取得
    bool isShiftPressed = io.KeyShift;
    bool isRightMouseDown = io.MouseDown[ImGuiMouseButton_Right];
    bool isLeftMouseDown = io.MouseDown[ImGuiMouseButton_Left];

    if (isLeftMouseDown || isRightMouseDown)
    {
        // Y軸回転（左右見渡し）
        angleY += moveX * 0.5f;

        // X軸回転（上下見渡し）
        angleX += moveY * 0.5f;

        // ジンバルロック（真上・真下での反転）を防ぐためにピッチ角を制限 (-89°〜 +89°)
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

    // 新しい前方・右・上ベクトル
    Vector3 Front = Vector3(-cx * sy, -sx, -cx * cy);
    Front.Normalize();
    Vector3 Right = Vector3(cy, 0, -sy);
    Right.Normalize();
    Vector3 Up = Right.Cross(Front);

    // ----------------------------------------------------
    // B. カメラの位置・注視点移動の計算
    // ----------------------------------------------------
    if (isRightMouseDown)
    {
        // ALTなし右ドラッグ中のみ、WASDキーによるFPS移動を許可
        float speed = 5.0f; // 基本移動速度（1秒間の移動距離）
        if (isShiftPressed)
        {
            speed *= 3.0f; // SHIFTでダッシュ（Unityのデフォルトに近い3倍）
        }

        Vector3 moveDir = Vector3::Zero;

        // ImGuiのキーマップ（ImGuiKey_Wなど）を使用して判定
        if (ImGui::IsKeyDown(ImGuiKey_W)) moveDir += Front;
        if (ImGui::IsKeyDown(ImGuiKey_S)) moveDir -= Front;
        if (ImGui::IsKeyDown(ImGuiKey_A)) moveDir += Right;
        if (ImGui::IsKeyDown(ImGuiKey_D)) moveDir -= Right;
        if (ImGui::IsKeyDown(ImGuiKey_E)) moveDir += Up;
        if (ImGui::IsKeyDown(ImGuiKey_Q)) moveDir -= Up;

        if (moveDir.LengthSquared() > 0.0f)
        {
            moveDir.Normalize();
            // 経過時間を掛けて位置（eye）を移動
            eye += moveDir * speed * elapsedTime;
        }

        // FPSモードは「自分の位置（eye）」が主役なので、現在の向きから注視点（focus）を逆算する
        focus = eye + (Front * distance);
    }
    else if (isLeftMouseDown)
    {
        // オービットモードは「注視点（focus）」が主役なので、位置（eye）を逆算する
        eye = focus - (Front * distance);
    }
    else
    {
        // マウスホイールによるズーム（Unityはいつでもホイールズームが可能）
        if (io.MouseWheel != 0.0f)
        {
            distance -= io.MouseWheel * distance * 0.1f;
            if (distance < 0.1f) distance = 0.1f; // めり込み防止
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

    // ----------------------------------------------------
    // C. 最終的な行列計算とパラメータ同期
    // ----------------------------------------------------
    Matrix View = Matrix::CreateLookAt(eye, focus, Up);
    Matrix World = DirectX::XMMatrixTranspose(View);

    // 次フレーム用に軸を更新
    right = Vector3::TransformNormal(Vector3(1, 0, 0), World);
    up = Vector3::TransformNormal(Vector3(0, 1, 0), World);
}

void FreeCameraController::OnFocusLost()
{
    // ウィンドウのフォーカスが外れた際、ImGuiの入力状態（押しっぱなし判定など）が
    // 残ってカメラが暴走するのを防ぐため、内部の入力を一度リセットする
    ImGui::GetIO().ClearEventsQueue();
}