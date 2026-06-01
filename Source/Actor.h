// Actor.h

#pragma once

#include "Transform.h"
#include "Object.h"

class Actor : public Object
{
public:
    Actor(std::string name = "", std::string tag = "", bool isActive = true, std::string layer = "")
        : Object(name, tag, isActive), layer(layer) {}
    virtual ~Actor() = default;

    Transform transform;

    // オーバーライドする必要があるやつだけ(transform割り込み)
	void Update() override;
	void DrawGUI() override;

    virtual void OnCollisionEnter(Actor* other) {}
    virtual void OnCollisionExit(Actor* other) {}
    virtual void OnTriggerEnter(Actor* other) {}
    virtual void OnTriggerExit(Actor* other) {}

    void SetLayer(const std::string& layer) { this->layer = layer; }
	const std::string& GetLayer() const { return layer; }

protected:
	std::string layer;
};
