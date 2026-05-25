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

	virtual void Update(float elapsedTime);
	virtual void Render(const RenderContext& rc, float elapsedTime);
	virtual void DrawGUI(float elapsedTime);

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
	virtual void OnUpdate(float elapsedTime) {}
	virtual void OnLateUpdate(float elapsedTime) {}
	virtual void OnRender(const RenderContext& rc, float elapsedTime) {}
	virtual void OnDrawGUI(float elapsedTime) {}

	bool isActive;
	std::string name;
	std::string tag;

	struct Components {
		std::vector<std::unique_ptr<Component>> data;
		void push_back(std::unique_ptr<Component> component);
		void Update(float elapsedTime);
		void LateUpdate(float elapsedTime);
		void Render(const RenderContext& rc, float elapsedTime);
		void DrawGUI(float elapsedTime);
	} componentList;
};