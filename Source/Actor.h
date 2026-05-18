// Actor.h

#pragma once

#include "Common.h"
#include "Component.h"
#include "Transform.h"

class Actor {
public:
	Actor(std::string name = "", std::string tag = "", std::string layer = "") : name(name), tag(tag), layer(layer) {}
    virtual ~Actor() = default;

    void Update(float elapsedTime);
    void Render(const RenderContext& rc, float elapsedTime);
    void DrawGUI(float elapsedTime);

    Transform transform;

    struct Components {
        std::vector<std::unique_ptr<Component>> data;
        void push_back(std::unique_ptr<Component> component);
        void Update(float elapsedTime);
        void LateUpdate(float elapsedTime);
        void Render(const RenderContext& rc, float elapsedTime);
        void DrawGUI(float elapsedTime);
    } componentList;

    template<typename T, typename... Args>
    T* AddComponent(Args&&... args) {
        componentList.push_back(std::make_unique<T>(this, std::forward<Args>(args)...));
        return static_cast<T*>(componentList.data.back().get());
    }

    template<typename T>
    T* GetComponent() {
        for (auto& c : componentList.data) {
            if (auto* p = dynamic_cast<T*>(c.get())) {
                return p;
            }
        }
        return nullptr;
    }

    virtual void OnCollisionEnter(Actor* other) {}
    virtual void OnCollisionExit(Actor* other) {}
    virtual void OnTriggerEnter(Actor* other) {}
    virtual void OnTriggerExit(Actor* other) {}

protected:
    virtual void OnUpdate(float elapsedTime) {}
    virtual void OnLateUpdate(float elapsedTime) {}
    virtual void OnRender(const RenderContext& rc, float elapsedTime) {}
    virtual void OnDrawGUI(float elapsedTime) {}

    std::string name;
	std::string tag;
	std::string layer;
};
