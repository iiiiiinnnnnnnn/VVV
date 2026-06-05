// Object.h

#pragma once

#include "Common.h"
#include "Components.h"

class Object
{
public:
	Object(const std::string& name = "", const std::string& tag = "", bool isActive = true)
		: name(name), tag(tag), isActive(isActive) {}
	
	void SetActive(bool active) { isActive = active; }
	void SetName(const std::string& name) { this->name = name; }
	void SetTag(const std::string& tag) { this->tag = tag; }

	bool IsActive() const { return isActive; }
	const std::string& GetName() const { return name; }
	const std::string& GetTag() const { return tag; }
	bool CompareTag(const std::string& otherTag) const { return tag == otherTag; }

	virtual void Update();
	virtual void Render(const RenderContext& rc);
	virtual void DrawGUI();

	void Destroy() { pendingDestroy = true; }
	bool IsPendingDestroy() const { return pendingDestroy; }

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

protected:
	virtual void OnUpdate() {}
	virtual void OnLateUpdate() {}
	virtual void OnRender(const RenderContext& rc) {}
	virtual void OnDrawGUI() {}

	bool isActive;
	std::string name;
	std::string tag;

	bool pendingDestroy = false;

	struct Components {
		std::vector<std::unique_ptr<Component>> data;
		void push_back(std::unique_ptr<Component> component);
		void Update();
		void LateUpdate();
		void Render(const RenderContext& rc);
		void DrawGUI();
	} componentList;
};