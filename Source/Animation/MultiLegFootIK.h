// MultiLegFootIK.h

#pragma once

#include <string>
#include <vector>

#include "Core/Foundation/Common.h"
#include "Core/Object/Component.h"
#include "Animation/FootIK.h"
#include "Resource/VMDLModel.h"

class Animator;

class MultiLegFootIK : public Component
{
public:
	MultiLegFootIK(Object* owner, LayerId layerId, VMDLModel* model, Animator* animator = nullptr);
	~MultiLegFootIK() override = default;

	void LateUpdate() override;
	void DrawGUI() override;
	const char* GetDebugName() const override { return ICON_FA_BONE " MultiLegFootIK"; }
	int GetUpdateOrder() const override { return 200; }

	void AddLeg(const char* rootName, const char* midName, const char* tipName, const char* contactName = nullptr);
	int AddLegsFromVmdlSettings();
	void SetRay(float up, float down, float contactOffset);
	bool HasGroundContact() const;

	void SetWaistNodeIndex(int index) { waistNodeIndex = index; }
	void SetModelVisualOffsetY(float y) { modelVisualOffsetY = y; }
	void SetContactOffset(float offset) { contactOffset = offset; }
	void SetMaxUpCorrection(float value) { maxUpCorrection = value; }
	void SetMaxDownCorrection(float value) { maxDownCorrection = value; }

private:
	void UpdateModelTransform();
	void ApplyFootSettings();
	float GetFootIKWeight(int footIndex) const;

	VMDLModel* model = nullptr;
	Animator* animator = nullptr;
	LayerId layerId = 0;
	int waistNodeIndex = -1;
	std::vector<FootIK*> footIKs;
	std::vector<std::vector<std::string>> footTargetNames;
	float modelVisualOffsetY = -0.06f;
	float rayUp = 1.0f;
	float rayDown = 5.0f;
	float contactOffset = 0.295f;
	float maxUpCorrection = 2.0f;
	float maxDownCorrection = 5.0f;
	bool showFootDebug = true;
};
