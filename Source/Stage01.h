// Stage01.h

#pragma once

#include "Components.h"
#include "Actor.h"

class Stage01 : public Actor
{
public:
	Stage01();
	~Stage01() = default;
	void OnUpdate() override;
	void OnRender(const RenderContext& rc);
	void OnDrawGUI() override;

private:

};
