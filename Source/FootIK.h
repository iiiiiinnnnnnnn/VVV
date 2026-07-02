// FootIK.h

#pragma once
#include <algorithm>
#include <cmath>
#include <cfloat>

#include "Common.h"
#include "Model.h"
#include "PhysicsComponent.h"

class FootIK : public PhysicsComponent
{
public:
	FootIK(
		Object* owner,
		LayerId layerId,
		Model* model,
		const char* thighName,
		const char* calfName,
		const char* footName,
		const char* ballName = nullptr);

	~FootIK() override = default;

	void Render(const RenderContext& rc) override;
	void DrawGUI() override;
	const char* GetDebugName() const override { return ICON_FA_BONE " FootIK"; }

	int GetUpdateOrder() const override { return 200; }

	bool UpdateGroundTarget(
		float rayUp = 1.0f,
		float rayDown = 3.0f,
		float contactOffset = 0.01f);

	void InitializeFromCurrentPose(float poleDistance = 0.5f);

	void SetTarget(const Vector3& targetPosition);
	void SetTargetFromContact(
		const Vector3& contactPosition,
		const Vector3& contactNormal,
		float contactOffset = 0.01f);

	void SetPoleWorldPosition(const Vector3& poleWorldPosition);
	void SetIKEnabled(bool enabled);
	bool IsIKEnabled() const { return chain.enabled; }
	void SyncPoleWorldPosition();
	void SyncPoleLocalPosition();

	Vector3 GetPoleWorldPosition() const;
	Vector3 GetTargetPosition() const;
	Vector3 GetContactWorldPosition() const;
	bool HasGroundContact() const { return hasGroundContact; }
	float GetGroundOffsetY() const { return groundOffsetY; }

	void SolveIK(const DirectX::XMFLOAT4X4& modelWorldTransform);

	bool IsPoleInitialized() const { return chain.poleInitialized; }

private:
	struct Chain
	{
		Model::Node* root = nullptr;    // thigh
		Model::Node* mid = nullptr;     // calf
		Model::Node* tip = nullptr;     // foot
		Model::Node* contact = nullptr; // ballB‚È‚¯‚ê‚Î foot

		Vector3 targetPosition = Vector3::Zero;
		Vector3 polePosition = Vector3::Zero;
		Vector3 poleLocalPosition = Vector3::Zero;

		bool poleInitialized = false;

		float weight = 1.0f;
		bool enabled = true;
	};

	Chain chain;
	Model* model = nullptr;

	Vector3 rayStart, rayEnd;
	bool hasGroundContact = false;
	float groundOffsetY = 0.0f;
	Vector3 smoothedTargetPosition = Vector3::Zero;
	bool hasSmoothedTarget = false;
	float smoothedGroundOffsetY = 0.0f;
	float targetSmoothSpeed = 24.0f;
	float groundOffsetSmoothSpeed = 18.0f;
	float ikBlendSpeed = 20.0f;
	int lostGroundFrameCount = 0;
	int maxLostGroundFrames = 10;

	void ResetGroundState();
	void KeepPreviousGroundTarget();
	void SetSmoothedTarget(const Vector3& targetPosition, float targetGroundOffsetY);

	static void UpdateWorldTransforms(Model::Node& node, const DirectX::XMFLOAT4X4& modelWorldTransform)
	{
		DirectX::XMMATRIX S = DirectX::XMMatrixScaling(node.scale.x, node.scale.y, node.scale.z);
		DirectX::XMMATRIX R = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&node.rotation));
		DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(node.position.x, node.position.y, node.position.z);
		DirectX::XMMATRIX LocalTransform = S * R * T;

		DirectX::XMMATRIX ParentGlobalTransform =
			node.parent != nullptr
			? DirectX::XMLoadFloat4x4(&node.parent->globalTransform)
			: DirectX::XMMatrixIdentity();

		DirectX::XMMATRIX GlobalTransform = LocalTransform * ParentGlobalTransform;
		DirectX::XMMATRIX WorldTransform = GlobalTransform * DirectX::XMLoadFloat4x4(&modelWorldTransform);

		DirectX::XMStoreFloat4x4(&node.localTransform, LocalTransform);
		DirectX::XMStoreFloat4x4(&node.globalTransform, GlobalTransform);
		DirectX::XMStoreFloat4x4(&node.worldTransform, WorldTransform);

		for (Model::Node* child : node.children)
		{
			UpdateWorldTransforms(*child, modelWorldTransform);
		}
	}

	static void RotateBone(Model::Node& bone, const Vector3& direction1, const Vector3& direction2)
	{
		Vector3 dir1 = direction1;
		Vector3 dir2 = direction2;

		if (dir1.Length() < 0.001f) return;
		if (dir2.Length() < 0.001f) return;

		dir1.Normalize();
		dir2.Normalize();

		Vector3 axis = dir1.Cross(dir2);
		if (axis.Length() < 0.001f)
		{
			return;
		}

		axis.Normalize();

		if (bone.parent != nullptr)
		{
			Matrix parentWorldTransform = bone.parent->worldTransform;
			Matrix inverseParentWorldTransform = parentWorldTransform.Invert();

			axis = Vector3::TransformNormal(axis, inverseParentWorldTransform);
			axis.Normalize();
		}

		float dot = dir1.Dot(dir2);
		dot = std::clamp(dot, -1.0f, 1.0f);

		float angle = std::acos(dot);
		if (angle <= FLT_EPSILON)
		{
			return;
		}

		Quaternion quat = Quaternion::CreateFromAxisAngle(axis, angle);

		bone.rotation *= quat;
		bone.rotation.Normalize();
	}
};

