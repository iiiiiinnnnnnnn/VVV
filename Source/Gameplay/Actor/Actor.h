// Actor.h
#pragma once

#include "Core/Object/Object.h"
#include "Core/Object/Transform.h"

class PhysicsComponent;

class Actor : public Object, public std::enable_shared_from_this<Actor>
{
public:
	Actor(std::string name = "", std::string tag = "", bool isActive = true,
		const Transform& transform = {})
        : Object(name, tag, isActive), transform(transform) {}
    ~Actor() override = default;

	void Update() override;
	void DrawGUI() override;
	void DrawGUI(bool selected);
	Transform* GetTransform() override { return &transform; }
	const Transform* GetTransform() const override { return &transform; }

	void Destroy(float delay = 0.0f) override;
	void SetSpawnTag(const std::string& value) { spawnTag = value; }
	const std::string& GetSpawnTag() const { return spawnTag; }

    virtual void OnCollisionEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) {}
    virtual void OnCollisionStay(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) {}
    virtual void OnCollisionExit(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) {}
    virtual void OnTriggerEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) {}
	virtual void OnTriggerStay(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) {}
    virtual void OnTriggerExit(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) {}

public:
    Transform transform;

private:
	std::string spawnTag;
};
