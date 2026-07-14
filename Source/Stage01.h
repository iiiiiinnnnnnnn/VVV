// Stage01.h

#pragma once

#include "Stage.h"

class Stage01 : public Stage
{
public:
	Stage01();
	void OnUpdate() override;
	void OnRender(const RenderContext& rc) override;
	void OnDrawGUI() override;

private:
	float aracoreSpawnTimer = -1.0f;
};
