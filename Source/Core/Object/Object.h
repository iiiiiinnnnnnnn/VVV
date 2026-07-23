// Object.h

#pragma once

#include "Core/Object/Component.h"
#include "Core/Object/Transform.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

struct RenderContext;
class RectTransform;

class Object
{
public:
	Object(const std::string& name = "", const std::string& tag = "", bool isActive = true)
		: name(name), tag(tag), isActive(isActive) { }
	virtual ~Object();

	virtual void Awake();
	virtual void Start();
	virtual void Update();
	virtual void LateUpdate();
	virtual void Render(const RenderContext& rc);
	virtual void DrawGUI();

	virtual void Destroy(float delay = 0.0f) { destroyTimer = delay; }
	bool IsPendingDestroy() const { return destroyTimer.has_value() && destroyTimer.value() <= 0.0f; }
	void SetComponentDebugVisible(bool visible) { componentDebugVisible = visible; }

	template<typename T, typename... Args>
	T* AddComponent(Args&&... args)
	{
		auto component = std::make_unique<T>(this, std::forward<Args>(args)...);
		auto* result = component.get();
		components.push_back(std::move(component));
		updateOrderDirty = true;

		if (isAwake)
		{
			result->Awake();
			result->isAwake = true;
		}

		if (isStarted && isActive && result->IsActive())
		{
			result->Start();
			result->isStarted = true;
		}

		return result;
	}

	template<typename T>
	T* GetComponent() const;

	template<typename T>
	std::vector<T*> GetComponents() const;

	// Name
	const std::string& GetName() const { return name; }
	void SetName(const std::string& name) { this->name = name; }

	// Tag
	const std::string& GetTag() const { return tag; }
	void SetTag(const std::string& tag) { this->tag = tag; }
	bool CompareTag(const std::string& otherTag) const { return tag == otherTag; }

	// Active
	bool IsActive() const { return isActive; }
	void SetActive(bool active) { isActive = active; }
	virtual Transform* GetTransform() { return nullptr; }
	virtual const Transform* GetTransform() const { return nullptr; }
	virtual RectTransform* GetRectTransform() { return nullptr; }
	virtual const RectTransform* GetRectTransform() const { return nullptr; }

protected:
	virtual void OnAwake() {}
	virtual void OnStart() {}
	virtual void OnUpdate() {}
	virtual void OnLateUpdate() {}
	virtual void OnRender(const RenderContext& rc) {}
	virtual void OnDrawGUI() {}

protected:
	std::string name, tag;
	bool isActive;
	std::optional<float> destroyTimer;
	std::vector<std::unique_ptr<Component>> components;
	bool isAwake = false;
	bool isStarted = false;
	bool componentDebugVisible = false;

private:
	void SortComponentsByUpdateOrder();
	bool updateOrderDirty = false;
};

#include "Core/Object/Object.inl"
