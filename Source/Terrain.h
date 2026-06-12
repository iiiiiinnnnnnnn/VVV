// Terrain.h

#pragma once

#include "Component.h"

class Terrain : public Component
{
public:
	Terrain();
	~Terrain();

	void Update() override;
	void Render(const RenderContext& rc) override;
	void DrawGUI() override;

private:

};