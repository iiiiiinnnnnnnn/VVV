// HumanoidFootIK.h

#pragma once

#include "Core/Foundation/Common.h"
#include "Resource/VMDLModel.h"
#include "Core/Object/Component.h"
#include "Animation/FootIK.h"

class Animator;

class HumanoidFootIK : public Component
{
public:
	HumanoidFootIK(
		Object* owner,
		LayerId layerId,
		VMDLModel* model,
		Animator* animator,
		const char* activeStateName,
		const char* pelvisName,
		const char* thighLName, const char* calfLName, const char* footLName, const char* ballLName,
		const char* thighRName, const char* calfRName, const char* footRName, const char* ballRName);
	~HumanoidFootIK() override = default;

	void LateUpdate() override;
	void DrawGUI() override;
	const char* GetDebugName() const override { return ICON_FA_BONE " HumanoidFootIK"; }
	int GetUpdateOrder() const override { return 200; }

	void SetRay(float up, float down, float contactOffset);
	void SetHipOffsetLimit(float minOffsetY, float maxOffsetY);

private:
	bool ShouldUseIK() const;
	float GetVmdlFootWeight(int footIndex) const;
	void ApplyHipOffset(const Vector3& baseHipLocalPosition);
	void ResetHipOffset(const Vector3& baseHipLocalPosition);

	VMDLModel* model = nullptr;
	Animator* animator = nullptr;
	FootIK* footIK_L = nullptr;
	FootIK* footIK_R = nullptr;
	int hipNodeIndex = -1;

	std::string activeStateName = "Idle";
	float rayUp = 0.2f;
	float rayDown = 0.5f;
	float contactOffset = 0.01f;
	float minHipOffsetY = -0.2f;
	float maxHipOffsetY = 0.0f;
	float visualHipOffsetY = 0.0f;
};
