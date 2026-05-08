// Component.h

#pragma once

#include "ModelRenderer.h"

class Actor;

class Component {
public:
    Component(Actor* owner) : owner(owner) {}
    virtual ~Component() = default;
    virtual void Update(float elapsedTime) {}
	virtual void Render(const RenderContext& rc, float elapsedTime, ModelRenderer* renderer) {}
	virtual void DrawGUI(float elapsedTime) {}

protected:
    Actor* owner;
};
