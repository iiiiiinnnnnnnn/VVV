// Actor.h

#pragma once

#include "RenderContext.h"
#include "Component.h"
#include "Transform.h"

class Actor {
public:
	Transform transform;

	struct Components {
		std::vector<std::unique_ptr<Component>> data;
		void push_back(std::unique_ptr<Component> component) { data.push_back(std::move(component)); }
		void Update(float elapsedTime) {
			for (auto& c : data) {
				c->Update(elapsedTime);
			}
		}
		void Render(const RenderContext& rc, float elapsedTime, ModelRenderer* renderer) {
			for (auto& c : data) {
				c->Render(rc, elapsedTime, renderer);
			}
		}
		void DrawGUI(float elapsedTime) {
			for (auto& c : data) {
				ImGui::PushID((void*)((uintptr_t)c.get() ^ (uintptr_t)this));
				c->DrawGUI(elapsedTime);
				ImGui::PopID();
			}
		}
	} componentList;

	virtual ~Actor() = default;
	
	void Update(float elapsedTime) {
		transform.Update();
		componentList.Update(elapsedTime);
		OnUpdate(elapsedTime);
	}

	void Render(const RenderContext& rc, float elapsedTime, ModelRenderer* renderer) {
		componentList.Render(rc, elapsedTime, renderer);
		OnRender(rc, elapsedTime);
	}

	void DrawGUI(float elapsedTime) {
		ImGui::SetNextWindowSize(ImVec2(350, 500), ImGuiCond_FirstUseEver);
		char title[64];
		sprintf(title, "Actor##%p", this);
		ImGui::Begin(title);
		if (ImGui::CollapsingHeader("Transform")) {
			ImGui::InputFloat3("Position", &transform.position.x);
			ImGui::InputFloat4("Rotation", &transform.rotation.x);
			ImGui::InputFloat3("Scale", &transform.scale.x);
		}
		componentList.DrawGUI(elapsedTime);
		if (ImGui::CollapsingHeader("User param")) {
			OnDrawGUI(elapsedTime);
		}
		ImGui::End();
	}

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
	virtual void OnUpdate(float elapsedTime) {};
	virtual void OnRender(const RenderContext& rc, float elapsedTime) {};
	virtual void OnDrawGUI(float elapsedTime) {};
};
