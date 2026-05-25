// Transform.cpp

#include "Transform.h"
#include "Components.h"
#include "Actor.h"

Transform::Transform(const Vector3& pos, const Quaternion& rot, const Vector3& sca)
	: position(pos), rotation(rot), scale(sca) {
}

Transform::Transform(Matrix m)
{
	m.Decompose(scale, rotation, position);
}

Transform Transform::FromPosition(const Vector3& position)
{
	Transform res(position);
	res.Update();
	return res;
}

Transform Transform::FromRotation(const Quaternion& rotation)
{
	Transform res(Vector3::Zero, rotation);
	res.Update();
	return res;
}

Transform Transform::FromAngle(const Vector3& euler)
{
	Transform res(Vector3::Zero, Quaternion::CreateFromYawPitchRoll(euler.y, euler.x, euler.z));
	res.Update();
	return res;
}

Transform Transform::FromScale(const Vector3& scale)
{
	Transform res(Vector3::Zero, Quaternion::Identity, scale);
	res.Update();
	return res;
}

Transform Transform::FromScale(float scale)
{
	Transform res(Vector3::Zero, Quaternion::Identity, Vector3(scale, scale, scale));
	res.Update();
	return res;
}

void Transform::Update() {
	matrix = Matrix::CreateScale(scale) * Matrix::CreateFromQuaternion(rotation) * Matrix::CreateTranslation(position);
	forward = Vector3::TransformNormal(Vector3::UnitZ, matrix);
	right = Vector3::TransformNormal(Vector3::UnitX, matrix);
}
