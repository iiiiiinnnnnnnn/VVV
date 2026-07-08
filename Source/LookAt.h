// LookAt.h

#pragma once

#include "Common.h"
#include "Component.h"

class Model;

class LookAt : public Component
{
public:
	LookAt(Object* owner, Model* model,
		const std::string& nodeName1,
		const std::string& nodeName2 = "",
		const std::string& nodeName3 = "");
	~LookAt() override = default;
	void LateUpdate() override;
	void Render(const RenderContext& rc) override;
	void DrawGUI() override;
	void SetTarget(const Vector3& target) { this->target = target; }
	Vector3 GetTarget() const { return target; }
	void SetMaxAngleYawDegrees(float degrees) { maxAngleYaw = RAD(degrees); }
	void SetMaxAnglePitchDegrees(float degrees) { maxAnglePitch = RAD(degrees); }
	void SetMaxAngleYawRadians(float radians) { maxAngleYaw = radians; }
	void SetMaxAnglePitchRadians(float radians) { maxAnglePitch = radians; }
	void SetSmoothSpeed(float speed) { smoothSpeed = speed; }
	float GetMaxAngleYawDegrees() const { return DEG(maxAngleYaw); }
	float GetMaxAnglePitchDegrees() const { return DEG(maxAnglePitch); }
	float GetSmoothSpeed() const { return smoothSpeed; }
	const char* GetDebugName() const override { return ICON_FA_EYE " LookAt"; }

private:
	Vector3 forward1, forward2, forward3;
	Vector3 right1, right2, right3;
	Vector3 up1, up2, up3;
	Vector3 target;
	float currentYaw1 = 0.0f, currentYaw2 = 0.0f, currentYaw3 = 0.0f;
	float currentPitch1 = 0.0f, currentPitch2 = 0.0f, currentPitch3 = 0.0f;
	Model* model;
	float maxAngleYaw = RAD(30.0f);
	float maxAnglePitch = RAD(45.0f);
	float smoothSpeed = 12.0f;
	int nodeIndex1 = -1, nodeIndex2 = -1, nodeIndex3 = -1;
};
