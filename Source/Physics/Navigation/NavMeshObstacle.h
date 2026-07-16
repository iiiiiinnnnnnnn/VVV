// NavMeshObstacle.h

#pragma once

#include "Core/Foundation/Common.h"
#include "Core/Object/Component.h"

class NavMeshObstacle : public Component
{
public:
	NavMeshObstacle(Object* owner) : Component(owner) {}

	void OnStart() override;
	void MarkDirty();
	bool GetBounds(Vector3& center, Vector3& size) const;

	void DrawGUI() override;
	const char* GetDebugName() const override { return ICON_FA_CUBE " NavMeshObstacle"; }
};
