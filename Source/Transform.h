// Transform.h

#pragma once

#include "Common.h"

class Actor;

struct Transform
{
	Vector3 position = Vector3::Zero;
	Quaternion rotation = Quaternion::Identity;
	Vector3 scale = Vector3::One;
	Vector3 forward = Vector3::Zero;
	Vector3 right = Vector3::Zero;
	Matrix matrix = Matrix::Identity;

	Actor* owner = nullptr;

	Transform(const Vector3& pos = Vector3::Zero, const Quaternion& rot = Quaternion::Identity, const Vector3& sca = Vector3::One);

	Transform(Matrix m);

	static Transform FromPosition(const Vector3& position);
	static Transform FromRotation(const Quaternion& rotation);
	static Transform FromAngle(const Vector3& euler);
	static Transform FromScale(const Vector3& scale);
	static Transform FromScale(float scale);

	// Updateの再計算が入るだけ
	void SetPosition(const Vector3& position);
	void SetRotation(const Quaternion& rotation);
	void SetAngle(const Vector3& euler);
	void SetScale(const Vector3& scale);
	void SetPosition(float x, float y, float z);
	void SetRotation(float x, float y, float z, float w);
	void SetAngle(float x, float y, float z);
	void SetScale(float x, float y, float z);
	void SetScale(float scale);

	void Update();
};
