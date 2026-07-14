// LookAt.cpp

#include "LookAt.h"
#include "Model.h"
#include "Actor.h"
#include "Graphics.h"
#include "RenderContext.h"
#include "GameTime.h"

LookAt::LookAt(Object* owner, Model* model,
	const std::string& nodeName1,
	const std::string& nodeName2,
	const std::string& nodeName3)
	: Component(owner), model(model)
{
	// 初期姿勢時のノードのローカル空間基準軸を求める

	auto SetBasis = [&](int nodeIndex, Vector3& forward, Vector3& right, Vector3& up)
	{
		if (nodeIndex < 0) return;

		auto& node = model->GetNodes().at(nodeIndex);
		Matrix inverseWorld = static_cast<Matrix>(node.worldTransform).Invert();

		forward = Vector3::TransformNormal(Vector3(0, 0, 1), inverseWorld);
		if (forward.Length() < 0.001f) forward = Vector3::Forward;
		forward.Normalize();

		up = Vector3::TransformNormal(Vector3::Up, inverseWorld);
		if (up.Length() < 0.001f) up = Vector3::Up;
		up.Normalize();

		right = up.Cross(forward);
		if (right.Length() < 0.001f) right = Vector3::Right;
		right.Normalize();

		up = forward.Cross(right);
		up.Normalize();
	};

	// ノード名からノードインデックスを取得し、基準軸を設定

	nodeIndex1 = model->GetNodeIndex(nodeName1.c_str());
	SetBasis(nodeIndex1, forward1, right1, up1);
	if (!nodeName2.empty())
	{
		nodeIndex2 = model->GetNodeIndex(nodeName2.c_str());
		SetBasis(nodeIndex2, forward2, right2, up2);
		if (!nodeName3.empty())
		{
			nodeIndex3 = model->GetNodeIndex(nodeName3.c_str());
			SetBasis(nodeIndex3, forward3, right3, up3);
		}
	}
}

void LookAt::LateUpdate()
{
	if (!model) return;
	if (nodeIndex1 < 0) return;

	// ウェイト分散

	float weight = 1.0f;
	if (nodeIndex2 != -1)
	{
		weight = 0.5f;
		if (nodeIndex3 != -1)
		{
			weight = 1.0f / 3.0f;
		}
	}

	// ノードのローカル空間でyaw/pitchに分解し、後ろ側には曲げない

	auto SetLookAt = [&](int nodeIndex, const Vector3& forward, const Vector3& right, const Vector3& up, float& currentYaw, float& currentPitch)
	{
		if (nodeIndex < 0) return;

		auto& node = model->GetNodes().at(nodeIndex);
		Matrix inverseWorld = static_cast<Matrix>(node.worldTransform).Invert();

		float yaw = 0.0f;
		float pitch = 0.0f;

		Vector3 localDirection = Vector3::Transform(target, inverseWorld);
		if (localDirection.Length() >= 0.001f)
		{
			localDirection.Normalize();

			float front = localDirection.Dot(forward);
			if (front > 0.001f)
			{
				float side = localDirection.Dot(right);
				float height = localDirection.Dot(up);
				float horizontalLength = sqrtf(front * front + side * side);
				if (horizontalLength < 0.001f) horizontalLength = 0.001f;

				yaw = atan2f(side, front);
				pitch = atan2f(height, horizontalLength);

				if (yaw > maxAngleYaw) yaw = maxAngleYaw;
				if (yaw < -maxAngleYaw) yaw = -maxAngleYaw;
				if (pitch > maxAnglePitch) pitch = maxAnglePitch;
				if (pitch < -maxAnglePitch) pitch = -maxAnglePitch;

				yaw *= weight;
				pitch *= weight;
			}
		}

		float rate = 1.0f - expf(-smoothSpeed * Game::Time::deltaTime);
		if (rate > 1.0f) rate = 1.0f;
		if (rate < 0.0f) rate = 0.0f;

		currentYaw += (yaw - currentYaw) * rate;
		currentPitch += (pitch - currentPitch) * rate;
		if (fabsf(currentYaw) < 0.001f && fabsf(currentPitch) < 0.001f) return;

		yaw = currentYaw;
		pitch = currentPitch;

		Quaternion yawRotation = Quaternion::CreateFromAxisAngle(up, yaw);
		Vector3 pitchAxis = Vector3::TransformNormal(
			right,
			Matrix::CreateFromQuaternion(yawRotation));
		if (pitchAxis.Length() < 0.001f) return;
		pitchAxis.Normalize();

		Quaternion pitchRotation = Quaternion::CreateFromAxisAngle(pitchAxis, -pitch);

		node.rotation *= yawRotation;
		node.rotation.Normalize();
		node.rotation *= pitchRotation;
		node.rotation.Normalize();
	};

	// ノードの回転を更新

	SetLookAt(nodeIndex1, forward1, right1, up1, currentYaw1, currentPitch1);
	if (nodeIndex2 != -1)
	{
		SetLookAt(nodeIndex2, forward2, right2, up2, currentYaw2, currentPitch2);
		if (nodeIndex3 != -1)
		{
			SetLookAt(nodeIndex3, forward3, right3, up3, currentYaw3, currentPitch3);
		}
	}

	model->UpdateTransform(dynamic_cast<Actor*>(owner)->transform.matrix);
}

void LookAt::Render(const RenderContext& rc)
{
	if (!showDebug) return;
	if (!model) return;
	if (nodeIndex1 < 0) return;

	Vector3 nodePosition = static_cast<Matrix>(
		model->GetNodes()[nodeIndex1].worldTransform).Translation();

	Game::Graphics::Instance().GetPrimitiveRenderer()->DrawLine(
		nodePosition,
		target,
		Color(1.0f, 1.0f, 0.0f, 1.0f),
		Color(1.0f, 1.0f, 0.0f, 1.0f));
}

void LookAt::DrawGUI()
{
	ImGui::DragFloat3("Target", &target.x, 0.01f);

	float maxAngleYawDegrees = DEG(maxAngleYaw);
	if (ImGui::DragFloat("Max Angle Yaw", &maxAngleYawDegrees, 0.1f, 0.0f, 180.0f))
	{
		maxAngleYaw = RAD(maxAngleYawDegrees);
	}

	float maxAnglePitchDegrees = DEG(maxAnglePitch);
	if (ImGui::DragFloat("Max Angle Pitch", &maxAnglePitchDegrees, 0.1f, 0.0f, 180.0f))
	{
		maxAnglePitch = RAD(maxAnglePitchDegrees);
	}

	ImGui::DragFloat("Smooth Speed", &smoothSpeed, 0.1f, 0.0f, 100.0f);
}
