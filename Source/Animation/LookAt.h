// LookAt.h

#pragma once

#include "Core/Foundation/Common.h"
#include "Core/Object/Component.h"

class VMDLModel;

class LookAt : public Component
{
public:
	LookAt(Object* owner, VMDLModel* model,
		const std::string& nodeName1,
		const std::string& nodeName2 = "",
		const std::string& nodeName3 = "");
	~LookAt() override = default;
	void Update() override;
	void LateUpdate() override;
	void Render(const RenderContext& rc) override;
	void DrawGUI() override;
	const char* GetDebugName() const override { return ICON_FA_EYE " LookAt"; }

	void SetTarget(const Vector3& target) { this->target = target; }
	Vector3 GetTarget() const { return target; }
	void SetFilterTags(const std::vector<std::string>& tags) { filterTags = tags; }
	const std::vector<std::string>& GetFilterTags() const { return filterTags; }
	void SetLookDistance(float distance) { lookDistance = distance; }
	float GetLookDistance() const { return lookDistance; }
	void SetMaxAngleYawDegrees(float degrees) { maxAngleYaw = RAD(degrees); }
	float GetMaxAngleYawDegrees() const { return DEG(maxAngleYaw); }
	void SetMaxAnglePitchDegrees(float degrees) { maxAnglePitch = RAD(degrees); }
	float GetMaxAnglePitchDegrees() const { return DEG(maxAnglePitch); }
	void SetMaxAngleYawRadians(float radians) { maxAngleYaw = radians; }
	float GetMaxAngleYawRadians() const { return maxAngleYaw; }
	void SetMaxAnglePitchRadians(float radians) { maxAnglePitch = radians; }
	float GetMaxAnglePitchRadians() const { return maxAnglePitch; }
	void SetSmoothSpeed(float speed) { smoothSpeed = speed; }
	float GetSmoothSpeed() const { return smoothSpeed; }

private:
	Vector3 forward1, forward2, forward3;
	Vector3 right1, right2, right3;
	Vector3 up1, up2, up3;
	Vector3 target;
	std::vector<std::string> filterTags;
	bool found = false;
	float lookDistance = 10.0f;
	float currentYaw1 = 0.0f, currentYaw2 = 0.0f, currentYaw3 = 0.0f;
	float currentPitch1 = 0.0f, currentPitch2 = 0.0f, currentPitch3 = 0.0f;
	VMDLModel* model;
	float maxAngleYaw = RAD(30.0f);
	float maxAnglePitch = RAD(20.0f);
	float smoothSpeed = 12.0f;
	int nodeIndex1 = -1, nodeIndex2 = -1, nodeIndex3 = -1;
};
