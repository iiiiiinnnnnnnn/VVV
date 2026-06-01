// Stage00.h

#pragma once

#include "Components.h"
#include "Actor.h"

class Stage00 : public Actor
{
public:
	Stage00();
	~Stage00() = default;
	void OnUpdate() override;
	void OnDrawGUI() override;

private:
	ShaderParamListWithMaterialName shaderParamWithMaterialName;
};
