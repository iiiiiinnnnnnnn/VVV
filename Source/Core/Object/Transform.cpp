#include "Core/Object/Transform.h"

#include <cmath>

#include "Core/Object/Object.h"
#include "IconsFontAwesome5.h"

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
	return res;
}

Transform Transform::FromRotation(const Quaternion& rotation)
{
	Transform res(Vector3::Zero, rotation);
	return res;
}

Transform Transform::FromAngle(const Vector3& euler)
{
	Transform res(Vector3::Zero, Quaternion::CreateFromYawPitchRoll(euler.y, euler.x, euler.z));
	return res;
}

Transform Transform::FromScale(const Vector3& scale)
{
	Transform res(Vector3::Zero, Quaternion::Identity, scale);
	return res;
}

Transform Transform::FromScale(float scale)
{
	Transform res(Vector3::Zero, Quaternion::Identity, Vector3(scale, scale, scale));
	return res;
}

void Transform::SetPosition(const Vector3& position)
{
	this->position = position;
}

void Transform::SetRotation(const Quaternion& rotation)
{
	this->rotation = rotation;
}

void Transform::SetAngle(const Vector3& euler)
{
	this->rotation = Quaternion::CreateFromYawPitchRoll(
		RAD(euler.y), RAD(euler.x), RAD(euler.z));
}

void Transform::SetScale(const Vector3& scale)
{
	this->scale = scale;
}

void Transform::SetPosition(float x, float y, float z)
{
	SetPosition(Vector3(x, y, z));
}

void Transform::SetRotation(float x, float y, float z, float w)
{
	SetRotation(Quaternion(x, y, z, w));
}

void Transform::SetAngle(float x, float y, float z)
{
	SetAngle(Vector3(x, y, z));
}

void Transform::SetScale(float x, float y, float z)
{
	SetScale(Vector3(x, y, z));
}

void Transform::SetScale(float scale)
{
	SetScale(Vector3(scale, scale, scale));
}

void Transform::SetDirection(
	const Vector3& direction,
	const Vector3& up)
{
	if (direction.LengthSquared() < 0.000001f)
	{
		return;
	}

	Vector3 forward = direction;
	forward.Normalize();

	Vector3 upDirection = up;

	if (upDirection.LengthSquared() < 0.000001f)
	{
		upDirection = Vector3::Up;
	}

	upDirection.Normalize();

	// 上方向と進行方向がほぼ平行だと回転行列を作れないので、
	// 別の上方向を使用する
	float dot = fabsf(
		forward.Dot(upDirection));

	if (dot > 0.999f)
	{
		upDirection =
			fabsf(forward.y) < 0.999f
			? Vector3::Up
			: Vector3::Right;
	}

	Matrix world =
		Matrix::CreateWorld(
		Vector3::Zero,
		forward,
		upDirection);

	rotation =
		Quaternion::CreateFromRotationMatrix(
		world);
}

Matrix Transform::GetMatrix() const
{
	return Matrix::CreateScale(scale)
		* Matrix::CreateFromQuaternion(rotation)
		* Matrix::CreateTranslation(position);
}

Vector3 Transform::GetForward() const
{
	Vector3 direction = Vector3::TransformNormal(
		Vector3::UnitZ, Matrix::CreateFromQuaternion(rotation));
	const float lengthSquared = direction.LengthSquared();
	if (!std::isfinite(lengthSquared) || lengthSquared <= 0.000001f) return Vector3::UnitZ;
	direction.Normalize();
	return direction;
}

Vector3 Transform::GetRight() const
{
	Vector3 direction = Vector3::TransformNormal(
		Vector3::UnitX, Matrix::CreateFromQuaternion(rotation));
	const float lengthSquared = direction.LengthSquared();
	if (!std::isfinite(lengthSquared) || lengthSquared <= 0.000001f) return Vector3::UnitX;
	direction.Normalize();
	return direction;
}

Transform::TransformChangedResult Transform::DrawGUI(bool hideScale)
{
	TransformChangedResult res{};

	if (ImGui::TreeNode((const char*)u8"トランスフォーム"))
	{
		res.positionChanged = ImGui::DragFloat3((const char*)u8"位置", &position.x, 0.1f);
		ImGui::SameLine();
		if (ImGui::Button((const char*)u8"原点##Position"))
		{
			position = Vector3::Zero;
			res.positionChanged = true;
		}
		Vector3 euler = rotation.ToEuler();
		euler.x = DEG(euler.x);
		euler.y = DEG(euler.y);
		euler.z = DEG(euler.z);
		if (ImGui::DragFloat3((const char*)u8"回転角度", &euler.x, 0.1f))
		{
			rotation = Quaternion::CreateFromYawPitchRoll(RAD(euler.y), RAD(euler.x), RAD(euler.z));
			rotation.Normalize();
			res.rotationChanged = true;
		}
		if (!hideScale)
			res.scaleChanged = ImGui::DragFloat3((const char*)u8"拡大率", &scale.x, 0.1f);
		ImGui::TreePop();
	}

	return res;
}
