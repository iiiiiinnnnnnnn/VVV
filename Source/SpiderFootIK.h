// SpiderFootIK.h

#pragma once

#include <vector>

#include "Common.h"
#include "Component.h"
#include "FootIK.h"
#include "Model.h"

class SpiderFootIK : public Component
{
public:
	SpiderFootIK(Object* owner, LayerId layerId, Model* model);
	~SpiderFootIK() override = default;

	void LateUpdate() override;
	void DrawGUI() override;
	const char* GetDebugName() const override { return ICON_FA_BONE " SpiderFootIK"; }
	int GetUpdateOrder() const override { return 200; }

	void AddLeg(const char* rootName, const char* midName, const char* tipName, const char* contactName = nullptr);
	void SetRay(float up, float down, float contactOffset);

private:
	void UpdateModelTransform();
	void ApplyRootRotationOffset();
	void ApplyFootSettings();

	Model* model = nullptr;
	LayerId layerId = 0;
	int waistNodeIndex = -1;
	std::vector<FootIK*> footIKs;
	float modelVisualOffsetY = -0.2f;
	float rayUp = 1.0f;
	float rayDown = 3.0f;
	float contactOffset = 0.01f;
	float rootCorrectionXDeg = -90.0f;
	bool showFootDebug = true;
};
