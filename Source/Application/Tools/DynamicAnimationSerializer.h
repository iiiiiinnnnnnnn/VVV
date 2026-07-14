// DynamicAnimationSerializer.h

#pragma once

#include "Application/Tools/DynamicAnimation.h"

#include <string>

class DynamicAnimationSerializer
{
public:
	static bool Save(
		const DynamicAnimationClip& clip,
		const std::string& path,
		std::string* errorMessage = nullptr);

	static bool Load(
		const std::string& path,
		DynamicAnimationClip& clip,
		std::string* errorMessage = nullptr);
};
