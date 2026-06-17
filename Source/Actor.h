// Actor.h

#pragma once

#include "Object.h"
#include "Transform.h"
#include "Components.h"
#include "GameDefine.h"

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

    virtual void OnCollisionEnter(Actor* other) {}
	virtual void OnCollisionStay(Actor* other) {}
    virtual void OnCollisionExit(Actor* other) {}
    virtual void OnTriggerEnter(Actor* other) {}
	virtual void OnTriggerStay(Actor* other) {}
    virtual void OnTriggerExit(Actor* other) {}

    void SetLayer(int layer) { this->layer = layer; }
    void SetTag(const std::string& tag) { this->tag = tag; }

	const int GetLayer() const { return layer; }
    const std::string& GetTag() const { return tag; }
    bool CompareTag(const std::string& otherTag) const { return tag == otherTag; }

protected:
    std::string tag;
	int layer;
};
