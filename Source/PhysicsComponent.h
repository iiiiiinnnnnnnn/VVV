// PhysicsComponent.h

#pragma once

#include "UserSettingsManager.h"
#include "Component.h"

class PhysicsComponent : public Component
{
public:
	PhysicsComponent(Object* owner, LayerId layerId) : Component(owner), layerId(layerId) {}
	~PhysicsComponent() = default;

	LayerId GetLayerId() const { return layerId; }
	void SetLayerId(LayerId id) { layerId = id; }
	Object* GetOwner() const { return owner; }

protected:
	LayerId layerId = 0;
};
