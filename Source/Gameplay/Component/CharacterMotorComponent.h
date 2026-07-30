// CharacterMotorComponent.h

#pragma once

#include <string>

#include "Core/Foundation/Common.h"
#include "Core/Object/Component.h"

class Animator;
class CharacterController;

class CharacterMotorComponent : public Component
{
public:
	CharacterMotorComponent(
		Object* owner,
		Animator* animator,
		CharacterController* characterController);

	void OnUpdate() override;
	void OnDrawGUI() override;
	const char* GetDebugName() const override { return ICON_FA_RUNNING " CharacterMotor"; }

	void SetRootMotionNode(const std::string& nodeName);
	void SetExternalVelocity(const Vector3& velocity) { externalVelocity = velocity; }
	void SetUseRootMotion(bool value) { useRootMotion = value; }
	void SetGravity(float value) { gravity = value; }
	void SetUseGravity(bool value) { useGravity = value; }
	void SetUseGroundSnap(bool value) { useGroundSnap = value; }
	void SetGroundSnapDistance(float upDistance, float downDistance);

	const Vector3& GetRootMotionDelta() const { return rootMotionDelta; }
	const Vector3& GetLastMoveDelta() const { return lastMoveDelta; }
	float GetVerticalVelocity() const { return verticalVelocity; }
	bool IsGrounded() const { return grounded; }

private:
	int GetUpdateOrder() const override { return 150; }
	bool RaycastGround(Vector3& position, Vector3& normal) const;
	void UpdateGravity();
	void SnapToGroundIfNeeded();

	Animator* animator = nullptr;
	CharacterController* characterController = nullptr;
	Vector3 externalVelocity = Vector3::Zero;
	Vector3 rootMotionDelta = Vector3::Zero;
	Vector3 lastMoveDelta = Vector3::Zero;
	float gravity = -9.81f;
	float verticalVelocity = 0.0f;
	float groundSnapUpDistance = 0.2f;
	float groundSnapDownDistance = 0.5f;
	float groundSnapSpeed = 30.0f;
	float minimumGroundNormalY = 0.35f;
	bool useGravity = true;
	bool useRootMotion = true;
	bool useGroundSnap = true;
	bool grounded = false;
};
