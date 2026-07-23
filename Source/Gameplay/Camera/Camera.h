// Camera.h
#pragma once

#include "Core/Foundation/Common.h"
#include "Core/Object/Component.h"

#include <cstdint>

// カメラ
class Camera : public Component
{
public:
	Camera(Object* owner, int priority = 0);

	void OnDrawGUI() override
	{
		int value = priority;
		if (ImGui::DragInt("Priority", &value))
			SetPriority(value);
	}
	const char* GetDebugName() const override { return ICON_FA_VIDEO " Camera"; }

	// 指定方向を向く
	void SetLookAt(const Vector3& eye, const Vector3& focus, const Vector3& up);

	// パースペクティブ設定
	void SetPerspectiveFov(float fovY, float aspect, float nearZ, float farZ);

	// ビュー行列取得
	const Matrix& GetView() const { return view; }

	// プロジェクション行列取得
	const Matrix& GetProjection() const { return projection; }

	// 視点取得
	const Vector3& GetEye() const { return eye; }

	// 注視点取得
	const Vector3& GetFocus() const { return focus; }

	// 上方向取得
	const Vector3& GetUp() const { return up; }

	// 前方向取得
	const Vector3& GetFront() const { return front; }

	// 右方向取得
	const Vector3& GetRight() const { return right; }

	int GetPriority() const { return priority; }
	void SetPriority(int value)
	{
		priority = value;
		priorityChanged = true;
	}

private:
	Matrix		view;
	Matrix		projection;

	Vector3		eye;
	Vector3		focus;

	Vector3		up;
	Vector3		front;
	Vector3		right;
	int priority = 0;
	bool priorityChanged = false;
	int normalizedPriority = 0;

	bool HasPriorityChange() const { return priorityChanged; }
	int GetNormalizedPriority() const { return normalizedPriority; }
	void SetNormalizedPriority(int value)
	{
		priority = value;
		normalizedPriority = value;
		priorityChanged = false;
	}

	friend class Stage;
};
