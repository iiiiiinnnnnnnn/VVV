// PhysicsComponent.h
#pragma once

#include <mutex>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

#include "Application/SettingsAndDebug/PhysicsLayerManager.h"
#include "Core/Object/Component.h"

class PhysicsComponent : public Component
{
public:
	PhysicsComponent(Object* owner, LayerId layerId, std::string name = {})
		: Component(owner), layerId(layerId), name(std::move(name))
	{
		std::lock_guard<std::mutex> lock(liveComponentsMutex);
		liveComponents.insert(this);
	}
	~PhysicsComponent() override
	{
		std::lock_guard<std::mutex> lock(liveComponentsMutex);
		liveComponents.erase(this);
	}

	static bool IsLive(const PhysicsComponent* collider)
	{
		if (!collider) return false;
		std::lock_guard<std::mutex> lock(liveComponentsMutex);
		return liveComponents.contains(collider);
	}

	LayerId GetLayerId() const { return layerId; }
	void SetLayerId(LayerId id) { layerId = id; }
	virtual bool IgnoresLayer(LayerId id) const { return false; }
	virtual bool IgnoresCollider(const PhysicsComponent& other) const
	{
		return IgnoresLayer(other.GetLayerId());
	}
	const std::string& GetName() const { return name; }
	bool CompareName(std::string_view value) const { return name == value; }
	void SetName(const std::string& value) { name = value; }
	Object* GetOwner() const { return owner; }

protected:
	LayerId layerId = 0;
	std::string name;

private:
	inline static std::mutex liveComponentsMutex;
	inline static std::unordered_set<const PhysicsComponent*> liveComponents;
};
