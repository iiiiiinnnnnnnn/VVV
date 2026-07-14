// Rigidbody.h

#pragma once

#include "Core/Object/Component.h"
#include "Physics/Core/PhysicsManager.h"
#include "Core/Object/Object.h"
#include "Core/Object/Transform.h"

class Rigidbody : public Component
{
public:
	using Component::Component;
	virtual PxRigidActor* GetRigidActor() const = 0;
	virtual void SetPosition(const Vector3& pos) = 0;
	virtual void SetRotation(const Quaternion& rot) = 0;

	void SetSceneEnabled(bool enabled)
	{
		auto* actor = GetRigidActor();
		if (!actor) return;

		auto* scene = PhysicsManager::Instance().GetSceneContext().GetScene();
		if (enabled && !actor->getScene()) scene->addActor(*actor);
		if (!enabled && actor->getScene()) scene->removeActor(*actor);
	}

	Vector3 GetPosition() const { return Conv::ToVector3(GetRigidActor()->getGlobalPose().p); }
	Quaternion GetRotation() const { return Conv::ToQuaternion(GetRigidActor()->getGlobalPose().q); }
};

#include "Physics/RigidBody/RigidbodyDynamic.h"
#include "Physics/RigidBody/RigidbodyStatic.h"
