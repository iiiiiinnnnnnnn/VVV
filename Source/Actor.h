// Actor.h

#pragma once

#include "Object.h"
#include "Transform.h"
#include "Components.h"
#include "GameDefine.h"
class ActorManager;
class Collider;

class Actor : public Object
{
public:
    Actor(std::string name = "", std::string tag = "", bool isActive = true, int layer = Layer::Default)
        : Object(name, isActive), tag(tag), layer(layer) {}
    virtual ~Actor() = default;

    Transform transform;

    // オーバーライドする必要があるやつだけ(transform割り込み)
	void Update() override;
	void DrawGUI() override;

    virtual void OnCollisionEnter(Collider* self, Collider* other, const Vector3& point, const Vector3& normal) {}
    virtual void OnCollisionStay(Collider* self, Collider* other, const Vector3& point, const Vector3& normal) {}
    virtual void OnCollisionExit(Collider* self, Collider* other, const Vector3& point, const Vector3& normal) {}
    virtual void OnTriggerEnter(Collider* self, Collider* other, const Vector3& point, const Vector3& normal) {}
	virtual void OnTriggerStay(Collider* self, Collider* other, const Vector3& point, const Vector3& normal) {}
    virtual void OnTriggerExit(Collider* self, Collider* other, const Vector3& point, const Vector3& normal) {}

	virtual void OnRegistered(ActorManager* actorManager) {}

    void SetLayer(int layer) { this->layer = layer; }
    void SetTag(const std::string& tag) { this->tag = tag; }

	const int GetLayer() const { return layer; }
    const std::string& GetTag() const { return tag; }
    bool CompareTag(const std::string& otherTag) const { return tag == otherTag; }
    bool IsDebugGUIOpen() const { return debugGUIOpen; }

    ActorManager* GetActorManager() const { return actorManager; }

protected:
	friend class ActorManager;
    std::string tag;
	int layer;
    bool debugGUIOpen = false;
    ActorManager* actorManager = nullptr;
};
