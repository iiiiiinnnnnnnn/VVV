// Component.h

#pragma once

#include "RenderContext.h"

class Object;

class Component {
public:
    Component(Object* owner) : owner(owner) {}
    virtual ~Component() = default;
    virtual void Update(float elapsedTime) {}
    virtual void LateUpdate(float elapsedTime) {}
	virtual void Render(const RenderContext& rc, float elapsedTime) {}
	virtual void DrawGUI(float elapsedTime) {}

protected:
    Object* owner;
};
