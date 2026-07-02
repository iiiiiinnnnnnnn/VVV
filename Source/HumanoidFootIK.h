// HumanoidFootIK.h

#pragma once

#include "Common.h"
#include "UserSettingsManager.h"
#include "Model.h"
#include "Component.h"

class HumanoidFootIK : public Component
{
public:
	HumanoidFootIK(
		Object* owner,
		LayerId layerId,
		Model* model,
		const char* pelvisName,
		const char* thighLName, const char* calfLName, const char* footLName, const char* ballLName,
		const char* thighRName, const char* calfRName, const char* footRName, const char* ballRName);
	~HumanoidFootIK() override = default;


};