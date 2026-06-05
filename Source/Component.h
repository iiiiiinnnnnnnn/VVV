// Component.h

#pragma once

#include "RenderContext.h"

class Object;

class Component {
public:
    Component(Object* owner) : owner(owner) {}
    virtual ~Component() = default;

    void SetActive(bool active) { isActive = active; }
    bool IsActive() const { return isActive; }

    virtual void Update() {}
    virtual void LateUpdate() {}
    virtual void Render(const RenderContext& rc) {}
    virtual void DrawGUI() {}

protected:
    Object* owner;
    bool isActive = true;
};