// Prop.h

#pragma once

#include "Actor.h"
#include "StageLoader.h"

class Prop : public Actor
{
public:
	Prop(StageLoader::PropData& propData);
	~Prop() = default;

	void OnUpdate() override;
};
