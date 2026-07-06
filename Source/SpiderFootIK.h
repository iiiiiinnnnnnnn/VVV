// SpiderFootIK.h

#pragma once

#include <string>
#include <vector>

#include "Common.h"
#include "Component.h"
#include "FootIK.h"
#include "Model.h"

class Animator;

class SpiderFootIK : public Component
{
public:
	SpiderFootIK(Object* owner, LayerId layerId, Model* model, Animator* animator = nullptr);
	~SpiderFootIK() override = default;

	void LateUpdate() override;
	void DrawGUI() override;
	const char* GetDebugName() const override { return ICON_FA_BONE " SpiderFootIK"; }
	int GetUpdateOrder() const override { return 200; }

	void AddLeg(const char* rootName, const char* midName, const char* tipName, const char* contactName = nullptr);
	void SetRay(float up, float down, float contactOffset);

private:
	void UpdateModelTransform();
	void ApplyFootSettings();
	float GetFootIKWeight(int footIndex) const;

	Model* model = nullptr;
	Animator* animator = nullptr;
	LayerId layerId = 0;
	int waistNodeIndex = -1;
	std::vector<FootIK*> footIKs;
	std::vector<std::vector<std::string>> footTargetNames;
	float modelVisualOffsetY = -0.06f;
	float rayUp = 1.0f;
	float rayDown = 5.0f;
	float contactOffset = 0.5f;
	float maxUpCorrection = 2.0f;
	float maxDownCorrection = 5.0f;
	bool showFootDebug = true;
};


