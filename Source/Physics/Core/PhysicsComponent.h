// PhysicsComponent.h

#pragma once

#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

#include "Application/SettingsAndDebug/UserSettingsManager.h"
#include "Core/Object/Component.h"

class PhysicsComponent : public Component
{
public:
	PhysicsComponent(Object* owner, LayerId layerId, std::string name = {})
		: Component(owner), layerId(layerId), name(std::move(name))
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
	const std::string& GetName() const { return name; }
	bool CompareName(std::string_view value) const { return name == value; }
	void SetName(const std::string& value) { name = value; }
	Object* GetOwner() const { return owner; }

protected:
	LayerId layerId = 0;
	std::string name;

private:
	inline static std::unordered_set<const PhysicsComponent*> liveComponents;
};
