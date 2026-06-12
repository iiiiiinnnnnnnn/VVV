// Component.h

#pragma once

#include "RenderContext.h"

// コンポーネントにはWidget、Actorどちらかしかアタッチできないものがある
// その場合はActorにdynamic_castした時にエラー吐くために変換&エラー関数がある

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

    Actor* GetOwnerAsActor(Object* owner_sub = nullptr);
    Widget* GetOwnerAsWidget(Object* owner_sub = nullptr);

protected:
    Object* owner;
    bool isActive = true;
};