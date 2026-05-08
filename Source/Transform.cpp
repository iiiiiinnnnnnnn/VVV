// Transform.cpp

#include "Transform.h"

Transform::Transform(const Vector3& pos, const Quaternion& rot, const Vector3& sca)
	: position(pos), rotation(rot), scale(sca) {
}

Transform::Transform(Matrix m)
{
	m.Decompose(scale, rotation, position);
}

void Transform::Update() {
	matrix = Matrix::CreateScale(scale) * Matrix::CreateFromQuaternion(rotation) * Matrix::CreateTranslation(position);
	forward = Vector3::TransformNormal(Vector3::UnitZ, matrix);
	right = Vector3::TransformNormal(Vector3::UnitX, matrix);
}