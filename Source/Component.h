// Component.h

#pragma once

#include "IconsFontAwesome5.h"

#include "imgui.h"
#include "imgui_stdlib.h"

struct RenderContext;
class Object;
class Actor;
class Widget;

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
    virtual int GetUpdateOrder() const { return 0; }

    Actor* GetOwnerAsActor(Object* owner_sub = nullptr) const;
    Widget* GetOwnerAsWidget(Object* owner_sub = nullptr) const;

	virtual const char* GetDebugName() const = 0;

protected:
    friend class Object;
    Object* owner;
    bool isActive = true;
	bool showDebug = false;
};
