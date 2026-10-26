#pragma once

#include "Core/Foundation/Common.h"

class Object;

struct Transform
{
	Vector3 position = Vector3::Zero;
	Quaternion rotation = Quaternion::Identity;
	Vector3 scale = Vector3::One;

	Transform(const Vector3& pos = Vector3::Zero, const Quaternion& rot = Quaternion::Identity, const Vector3& sca = Vector3::One);

	Transform(Matrix m);

	static Transform FromPosition(const Vector3& position);
	static Transform FromRotation(const Quaternion& rotation);
	static Transform FromAngle(const Vector3& euler);
	static Transform FromScale(const Vector3& scale);
	static Transform FromScale(float scale);

	// 位置・回転・スケールを設定する
	void SetPosition(const Vector3& position);
	void SetRotation(const Quaternion& rotation);
	void SetAngle(const Vector3& euler);
	void SetScale(const Vector3& scale);
	void SetPosition(float x, float y, float z);
	void SetRotation(float x, float y, float z, float w);
	void SetAngle(float x, float y, float z);
	void SetScale(float x, float y, float z);
	void SetScale(float scale);

	void SetDirection(
		const Vector3& direction,
		const Vector3& up = Vector3::Up);

	Matrix GetMatrix() const;
	// 回転から方向を取得する。スケールには依存しない。
	Vector3 GetForward() const;
	Vector3 GetRight() const;

	struct TransformChangedResult
	{
		bool positionChanged = false;
		bool rotationChanged = false;
		bool scaleChanged = false;
	};
	TransformChangedResult DrawGUI(bool hideScale = false);
};
