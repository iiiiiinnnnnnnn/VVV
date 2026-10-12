#pragma once

#include "Physics/RigidBody/Rigidbody.h"

class RigidbodyStatic : public Rigidbody {
public:
	using Rigidbody::Rigidbody;
	~RigidbodyStatic();
	void OnAwake() override;
	void OnEnabled() override;
	void OnDisabled() override;

public:
	const char* GetDebugName() const override { return ICON_FA_WEIGHT_HANGING " RigidbodyStatic"; }
	PxRigidActor* GetRigidActor() const override { return rb; }

	Vector3 GetPosition() const { return Conv::ToVector3(rb->getGlobalPose().p); }
	void SetPosition(const Vector3& pos);
	void SetPosition(float x, float y, float z);

	Quaternion GetRotation() const { return Conv::ToQuaternion(rb->getGlobalPose().q); }
	void SetRotation(const Quaternion& rot);
	void SetRotation(float x, float y, float z, float w);
	void SetRotation(float yaw, float pitch, float roll);

protected:
	PxRigidStatic* rb = nullptr;
};
