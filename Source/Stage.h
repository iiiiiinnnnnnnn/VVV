// Stage.h

#pragma once

#include "Common.h"
#include "Actor.h"

class Stage : public Actor
{
public:
	Stage();
	~Stage() = default;
	void OnUpdate(float elapsedTime) override;
	void OnRender(const RenderContext& rc, float elapsedTime) override;

private:

};
