// PhysicsComponent.h

#pragma once

#include <unordered_set>

#include "Application/SettingsAndDebug/UserSettingsManager.h"
#include "Core/Object/Component.h"

class PhysicsComponent : public Component
{
public:
	PhysicsComponent(Object* owner, LayerId layerId) : Component(owner), layerId(layerId)
	{
		liveComponents.insert(this);
	}
	~PhysicsComponent() override
	{
		liveComponents.erase(this);
	}

	static bool IsLive(const PhysicsComponent* collider)
	{
		return collider && liveComponents.contains(collider);
	}

	LayerId GetLayerId() const { return layerId; }
	void SetLayerId(LayerId id) { layerId = id; }
	Object* GetOwner() const { return owner; }

protected:
	LayerId layerId = 0;

private:
	inline static std::unordered_set<const PhysicsComponent*> liveComponents;
};
