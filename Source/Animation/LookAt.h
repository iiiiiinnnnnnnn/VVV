#pragma once

#include <array>

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
	const char* GetDebugName() const override { return ICON_FA_EYE " 注視点"; }

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
	struct LookNode
	{
		int nodeIndex = -1;
		Vector3 forward = Vector3::UnitZ;
		Vector3 right = Vector3::UnitX;
		Vector3 up = Vector3::UnitY;
		float currentYaw = 0.0f;
		float currentPitch = 0.0f;
	};

	void InitializeNode(LookNode& lookNode, const std::string& nodeName, const Matrix& modelRotation);
	void UpdateNode(LookNode& lookNode, float weight, float rate);

	std::array<LookNode, 3> lookNodes;
	Vector3 target;
	std::vector<std::string> filterTags;
	bool found = false;
	float lookDistance = 60.0f;
	VMDLModel* model;
	float maxAngleYaw = RAD(160.0f);
	float maxAnglePitch = RAD(0.0f);
	float smoothSpeed = 6.0f;
};
