// Component.h

#pragma once

#include "RenderContext.h"

class Actor;

class Component {
public:
    Component(Actor* owner) : owner(owner) {}
    virtual ~Component() = default;
    virtual void Update(float elapsedTime) {}
	virtual void Render(const RenderContext& rc, float elapsedTime) {}
	virtual void DrawGUI(float elapsedTime) {}

protected:
    Actor* owner;
};
