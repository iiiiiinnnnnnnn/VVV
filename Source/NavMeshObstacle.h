// NavMeshObstacle.h

#pragma once

#include "Common.h"
#include "Component.h"

class NavMeshObstacle : public Component
{
public:
	NavMeshObstacle(Object* owner) : Component(owner) {}

	bool GetBounds(Vector3& center, Vector3& size) const;

	void DrawGUI() override;
};
