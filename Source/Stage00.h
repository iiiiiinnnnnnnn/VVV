// Stage00.h

#pragma once

#include "Components.h"
#include "Actor.h"
#include "PBRShader.h"

class Stage00 : public Actor
{
public:
	Stage00();
	~Stage00() = default;
	void OnUpdate(float elapsedTime) override;
	void OnRender(const RenderContext& rc, float elapsedTime) override;
	void OnDrawGUI(float elapsedTime) override;

private:
	PBRShader::Params shaderParam;
};
