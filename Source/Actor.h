// Actor.h

#pragma once

#include "Object.h"
#include "Transform.h"

#include <string>

class ActorManager;
class PhysicsComponent;

class Actor : public Object
{
public:
    Actor(std::string name = "", std::string tag = "", bool isActive = true)
        : Object(name, tag, isActive) {}
    virtual ~Actor() = default;

    Transform transform;

    // オーバーライドする必要があるやつだけ(transform割り込み)
	void Update() override;
	void DrawGUI() override;

    virtual void OnCollisionEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) {}
    virtual void OnCollisionStay(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) {}
    virtual void OnCollisionExit(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) {}
    virtual void OnTriggerEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) {}
	virtual void OnTriggerStay(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) {}
    virtual void OnTriggerExit(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) {}

	virtual void OnRegistered(ActorManager* actorManager) {}

    ActorManager* GetActorManager() const { return actorManager; }
	Actor* FindActorByTag(const std::string& searchTag) const;

protected:
	friend class ActorManager;
    ActorManager* actorManager = nullptr;
};
