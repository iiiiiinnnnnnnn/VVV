// Transform.h

#pragma once

#include "Common.h"

struct Transform
{
	Vector3 position = Vector3::Zero;
	Quaternion rotation = Quaternion::Identity;
	Vector3 scale = Vector3::One;
	Vector3 forward = Vector3::Zero;
	Vector3 right = Vector3::Zero;
	Matrix matrix = Matrix::Identity;

	Transform(const Vector3& pos = Vector3::Zero, const Quaternion& rot = Quaternion::Identity, const Vector3& sca = Vector3::One);

	Transform(Matrix m);

	void Update();
};
