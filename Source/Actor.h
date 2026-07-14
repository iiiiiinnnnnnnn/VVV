// Actor.h

#pragma once

#include "Object.h"
#include "Transform.h"

class PhysicsComponent;

class Actor : public Object
{
public:
    Actor(std::string name = "", std::string tag = "", bool isActive = true)
        : Object(name, tag, isActive) {}
    virtual ~Actor() = default;

	void Update() override;
	void DrawGUI() override;
	Transform* GetTransform() override { return &transform; }
	const Transform* GetTransform() const override { return &transform; }

	void Destroy(float delay = 0.0f) override;

    virtual void OnCollisionEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) {}
    virtual void OnCollisionStay(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) {}
    virtual void OnCollisionExit(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) {}
    virtual void OnTriggerEnter(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) {}
	virtual void OnTriggerStay(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) {}
    virtual void OnTriggerExit(PhysicsComponent* self, PhysicsComponent* other, const Vector3& point, const Vector3& normal) {}

public:
    Transform transform;
};

