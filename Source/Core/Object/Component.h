// Component.h

#pragma once

// Component.h

#include "IconsFontAwesome5.h"
#include "imgui.h"
#include "imgui_stdlib.h"

struct RenderContext;
class Object;

class Component {
public:
    Component(Object* owner) : owner(owner) {}
    virtual ~Component() = default;

    virtual void OnAwake() {}
    virtual void OnStart() {}
    virtual void OnUpdate() {}
    virtual void OnLateUpdate() {}
    virtual void OnEnabled() {}
    virtual void OnDisabled() {}
	virtual void OnRender(const RenderContext& rc) {}
    virtual void OnDrawGUI() {}

    virtual void Awake() { OnAwake(); }
    virtual void Start() { OnStart(); }
    virtual void Update() { OnUpdate(); }
    virtual void LateUpdate() { OnLateUpdate(); }
    virtual void Render(const RenderContext& rc) { OnRender(rc); }
    virtual void DrawGUI() { OnDrawGUI(); }

    bool IsActive() const { return isActive; }
	void SetShowDebug(bool enabled) { showDebug = enabled; }
    void SetActive(bool active)
    {
        if (isActive == active)
            return;

        isActive = active;

        if (isActive)
            OnEnabled();
        else
            OnDisabled();
    }

public:
    virtual const char* GetDebugName() const = 0;

protected:
    friend class Object;
    virtual int GetUpdateOrder() const { return 0; }

protected:
    Object* owner;
    bool isActive = true;
	bool isAwake = false;
	bool isStarted = false;
	bool showDebug = false;
};
