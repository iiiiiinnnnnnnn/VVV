#pragma once

#include "Physics/RigidBody/Rigidbody.h"

class RigidbodyDynamic : public Rigidbody {
public:
	using Rigidbody::Rigidbody;
	~RigidbodyDynamic();
	void OnAwake() override;
	void OnUpdate() override;
	void OnLateUpdate() override;
	void OnEnabled() override;
	void OnDisabled() override;
	void OnDrawGUI() override;

public:
	const char* GetDebugName() const override { return ICON_FA_WEIGHT_HANGING " RigidbodyDynamic"; }
	PxRigidActor* GetRigidActor() const override { return rb; }

	Vector3 GetPosition() const { return Conv::ToVector3(rb->getGlobalPose().p); }
	void SetPosition(const Vector3& pos);
	void SetPosition(float x, float y, float z);

	Quaternion GetRotation() const { return Conv::ToQuaternion(rb->getGlobalPose().q); }
	void SetRotation(const Quaternion& rot);
	void SetRotation(float x, float y, float z, float w);
	void SetRotation(float yaw, float pitch, float roll);

	void SetKinematic(bool isKinematic);
	void SetMaxDepenetrationVelocity(float velocity);
	void AddForce(const Vector3& force);

	const Vector3 GetVelocity() const { return Conv::ToVector3(rb->getLinearVelocity()); }
	void SetVelocity(const Vector3& v);
	void SetAngularVelocity(const Vector3& v);

protected:
	PxRigidDynamic* rb = nullptr;
	bool isKinematic = false;
	float maxDepenetrationVelocity = -1.0f;
};
