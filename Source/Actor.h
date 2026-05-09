// Actor.h

#pragma once

#include "Common.h"
#include "RenderContext.h"
#include "Component.h"
#include "Transform.h"

class Actor {
public:
    Transform transform;

    struct Components {
        std::vector<std::unique_ptr<Component>> data;
        void push_back(std::unique_ptr<Component> component);
        void Update(float elapsedTime);
        void Render(const RenderContext& rc, float elapsedTime, ModelRenderer* renderer);
        void DrawGUI(float elapsedTime);
    } componentList;

    virtual ~Actor() = default;

    void Update(float elapsedTime);
    void Render(const RenderContext& rc, float elapsedTime, ModelRenderer* renderer);
    void DrawGUI(float elapsedTime);

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
    virtual void OnRender(const RenderContext& rc, float elapsedTime) {}
    virtual void OnDrawGUI(float elapsedTime) {}
};
