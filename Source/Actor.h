// Actor.h

#pragma once

#include "Transform.h"
#include "Object.h"
#include "GameDefine.h"

class Actor : public Object
{
public:
    Actor(std::string name = "", std::string tag = "", bool isActive = true, int layer = Layer::Default)
        : Object(name, tag, isActive), layer(layer) {}
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
	const int GetLayer() const { return layer; }

protected:
	int layer;
};
