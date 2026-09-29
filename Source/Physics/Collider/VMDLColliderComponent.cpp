// VMDLColliderComponent.cpp
#include "Physics/Collider/VMDLColliderComponent.h"

#include <algorithm>

#include "Gameplay/Actor/Actor.h"
#include "IconsFontAwesome5.h"
#include "Rendering/Core/Graphics.h"
#include "Rendering/Core/RenderContext.h"
#include "Resource/VMDLModel.h"

namespace
{
Matrix MakeColliderWorld(const VMDLModel& model, const Matrix& nodeWorld, const Matrix& offset)
{
	return model.GetScaledAttachmentTransform(offset * nodeWorld);
}
} // namespace

VMDLColliderComponent::VMDLColliderComponent(Object* owner, LayerId layerId, VMDLModel* model,
	int nodeIndex, int shapeType, const Vector3& size, const Matrix& offset, PxMaterial* material,
	bool isTrigger)
	: PhysicsComponent(owner, layerId), model(model), nodeIndex(nodeIndex), shapeType(shapeType),
	  size(size), offset(offset), isTrigger(isTrigger),
	  material(material ? material : PhysicsManager::Instance().GetDefaultMaterial())
{
	_ASSERT_EXPR(model != nullptr, L"requires model.");
	_ASSERT_EXPR(nodeIndex >= 0, L"invalid nodeIndex.");
	_ASSERT_EXPR(model && nodeIndex < static_cast<int>(model->GetNodes().size()),
		L"nodeIndex out of range.");

	if (!model || nodeIndex < 0 || nodeIndex >= static_cast<int>(model->GetNodes().size()))
	{
		return;
	}

	PxPhysics* physics = PhysicsManager::Instance().GetPhysics();
	const Matrix world =
		MakeColliderWorld(*model, model->GetNodes()[nodeIndex].worldTransform, offset);

	ghostActor = physics->createRigidDynamic(Conv::ToPxTransform(world));
	ghostActor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
	ghostActor->setRigidBodyFlag(PxRigidBodyFlag::eUSE_KINEMATIC_TARGET_FOR_SCENE_QUERIES, true);
	ghostActor->userData = owner;

	UpdateScaledSize(world);
	CreateShape();
}

VMDLColliderComponent::~VMDLColliderComponent()
{
	if (!ghostActor)
	{
		return;
	}

	if (shape)
	{
		ghostActor->detachShape(*shape);
		shape->release();
		shape = nullptr;
	}

	if (PxScene* scene = ghostActor->getScene())
	{
		scene->removeActor(*ghostActor);
	}

	ghostActor->release();
	ghostActor = nullptr;
}

void VMDLColliderComponent::CreateShape()
{
	if (!ghostActor)
	{
		return;
	}

	if (shape)
	{
		ghostActor->detachShape(*shape);
		shape->release();
		shape = nullptr;
	}

	PxPhysics* physics = PhysicsManager::Instance().GetPhysics();

	if (shapeType >= 3)
	{
		CreateMeshShape();
	}
	else switch (shapeType)
	{
	case 1:
		shape =
			physics->createShape(PxSphereGeometry(std::max(0.001f, scaledSize.x)), *material, true);
		break;

	case 2:
		shape = physics->createShape(PxCapsuleGeometry(std::max(0.001f, scaledSize.x),
										 std::max(0.001f, scaledSize.y) * 0.5f),
			*material, true);
		break;

	default:
		shape = physics->createShape(
			PxBoxGeometry(std::max(0.001f, scaledSize.x), std::max(0.001f, scaledSize.y),
				std::max(0.001f, scaledSize.z)),
			*material, true);
		break;
	}

	if (!shape)
	{
		return;
	}

	shape->userData = this;
	shape->setLocalPose(GetShapeLocalPose());
	shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, !isTrigger);
	shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, isTrigger);
	PhysicsManager::SetLayerToShape(shape, layerId);
	ghostActor->attachShape(*shape);

	if (IsActive() && !ghostActor->getScene())
	{
		PhysicsManager::Instance().GetSceneContext().GetScene()->addActor(*ghostActor);
	}
}

void VMDLColliderComponent::CreateMeshShape()
{
	if (!model || nodeIndex < 0 || nodeIndex >= static_cast<int>(model->GetNodes().size())) return;
	const int meshIndex = shapeType - 3;
	const auto& meshes = model->GetMeshes();
	if (meshIndex < 0 || meshIndex >= static_cast<int>(meshes.size())) return;

	const auto& mesh = meshes[meshIndex];
	if (mesh.vertices.size() < 3 || mesh.indices.size() < 3) return;
	const auto& nodes = model->GetNodes();
	const Matrix meshNodeTransform = mesh.node ? mesh.node->globalTransform : Matrix::Identity;
	const Matrix meshToCollider =
		meshNodeTransform * nodes[nodeIndex].globalTransform.Invert();
	std::vector<PxVec3> vertices;
	vertices.reserve(mesh.vertices.size());
	for (const auto& vertex : mesh.vertices)
	{
		Vector3 position = Vector3::Transform(vertex.position, meshToCollider);
		position *= scaledSize;
		vertices.emplace_back(position.x, position.y, position.z);
	}

	PxCookingParams* cooking = PhysicsManager::Instance().GetCooking();
	PxPhysics* physics = PhysicsManager::Instance().GetPhysics();
	if (!cooking || !physics) return;
	PxDefaultMemoryOutputStream cookedData;
	if (isTrigger)
	{
		PxConvexMeshDesc description;
		description.points.count = static_cast<PxU32>(vertices.size());
		description.points.stride = sizeof(PxVec3);
		description.points.data = vertices.data();
		description.flags = PxConvexFlag::eCOMPUTE_CONVEX | PxConvexFlag::eQUANTIZE_INPUT;
		description.quantizedCount = 255;
		if (!PxCookConvexMesh(*cooking, description, cookedData) || cookedData.getSize() == 0)
			return;
		PxDefaultMemoryInputData input(cookedData.getData(), cookedData.getSize());
		PxConvexMesh* convexMesh = physics->createConvexMesh(input);
		if (!convexMesh) return;
		shape = physics->createShape(PxConvexMeshGeometry(convexMesh), *material, true);
		convexMesh->release();
		return;
	}

	PxTriangleMeshDesc description;
	description.points.count = static_cast<PxU32>(vertices.size());
	description.points.stride = sizeof(PxVec3);
	description.points.data = vertices.data();
	description.triangles.count = static_cast<PxU32>(mesh.indices.size() / 3);
	description.triangles.stride = sizeof(uint32_t) * 3;
	description.triangles.data = mesh.indices.data();
	if (!PxCookTriangleMesh(*cooking, description, cookedData) || cookedData.getSize() == 0) return;
	PxDefaultMemoryInputData input(cookedData.getData(), cookedData.getSize());
	PxTriangleMesh* triangleMesh = physics->createTriangleMesh(input);
	if (!triangleMesh) return;
	shape = physics->createShape(PxTriangleMeshGeometry(triangleMesh), *material, true);
	triangleMesh->release();
}

void VMDLColliderComponent::UpdateScaledSize(const Matrix&)
{
	scaledSize = model ? model->GetScaledAttachmentVector(size) : size;
}

PxTransform VMDLColliderComponent::GetShapeLocalPose() const
{
	if (shapeType == 2)
	{
		return PxTransform(
			PxVec3(0.0f, 0.0f, 0.0f), PxQuat(DirectX::XM_PIDIV2, PxVec3(0.0f, 0.0f, 1.0f)));
	}

	return PxTransform(PxIdentity);
}

void VMDLColliderComponent::OnEnabled()
{
	if (!ghostActor || ghostActor->getScene())
	{
		return;
	}

	PhysicsManager::Instance().GetSceneContext().GetScene()->addActor(*ghostActor);
}

void VMDLColliderComponent::OnDisabled()
{
	sweepReady = false;
	if (!ghostActor)
	{
		return;
	}

	if (PxScene* scene = ghostActor->getScene())
	{
		scene->removeActor(*ghostActor);
	}
}

void VMDLColliderComponent::UpdateFromNode()
{
	if (!ghostActor || !model || nodeIndex < 0)
	{
		return;
	}

	const auto& nodes = model->GetNodes();
	if (nodeIndex >= static_cast<int>(nodes.size()))
	{
		return;
	}

	const Matrix world = MakeColliderWorld(*model, nodes[nodeIndex].worldTransform, offset);
	const PxTransform targetActorPose = Conv::ToPxTransform(world);
	const Vector3 previousScaledSize = scaledSize;
	UpdateScaledSize(world);
	if ((scaledSize - previousScaledSize).LengthSquared() > 0.000001f) CreateShape();

	if (!IsActive())
	{
		ghostActor->setGlobalPose(targetActorPose, false);
		sweepReady = true;
		return;
	}
	if (!sweepReady)
	{
		ghostActor->setGlobalPose(targetActorPose, false);
		sweepReady = true;
		return;
	}

	if (isTrigger && shape && shapeType < 3)
	{
		const PxTransform startPose = PxShapeExt::getGlobalPose(*shape, *ghostActor);
		const PxTransform endPose = targetActorPose * shape->getLocalPose();
		const PxVec3 movement = endPose.p - startPose.p;
		const float rotationDot = std::abs(startPose.q.dot(endPose.q));
		if (movement.magnitudeSquared() > 0.00000001f || rotationDot < 0.999999f)
		{
			const PxGeometryHolder geometry = shape->getGeometry();
			PhysicsManager::Instance().GetSceneContext().QueueTriggerSweep(
				this, geometry.any(), startPose, endPose, layerId, ghostActor);
		}
	}
	ghostActor->setKinematicTarget(targetActorPose);
}

Vector3 VMDLColliderComponent::GetWorldPosition() const
{
	if (!ghostActor)
	{
		return Vector3::Zero;
	}

	if (!shape)
	{
		return Conv::ToVector3(ghostActor->getGlobalPose().p);
	}

	return Conv::ToVector3(PxShapeExt::getGlobalPose(*shape, *ghostActor).p);
}

Actor* VMDLColliderComponent::FindOverlapActorByTag(const std::string& tag) const
{
	if (!ghostActor || !shape)
	{
		return nullptr;
	}

	const PxGeometryHolder geometry = shape->getGeometry();
	const PxTransform pose = PxShapeExt::getGlobalPose(*shape, *ghostActor);
	PxOverlapBuffer hit;

	const bool hitAny =
		PhysicsManager::Instance().GetSceneContext().GetScene()->overlap(geometry.any(), pose, hit);

	if (!hitAny)
	{
		return nullptr;
	}

	for (PxU32 i = 0; i < hit.getNbAnyHits(); ++i)
	{
		PxShape* hitShape = hit.getAnyHit(i).shape;
		if (!hitShape || hitShape == shape)
		{
			continue;
		}

		auto* collider = static_cast<PhysicsComponent*>(hitShape->userData);
		if (!PhysicsComponent::IsLive(collider))
		{
			continue;
		}

		Actor* actor = dynamic_cast<Actor*>(collider->GetOwner());
		if (!actor || actor == dynamic_cast<Actor*>(owner))
		{
			continue;
		}

		if (actor->CompareTag(tag))
		{
			return actor;
		}
	}

	return nullptr;
}

void VMDLColliderComponent::Render(const RenderContext& rc)
{
	if (!showDebug || !ghostActor)
	{
		return;
	}

	const PxTransform pose =
		shape ? PxShapeExt::getGlobalPose(*shape, *ghostActor) : ghostActor->getGlobalPose();

	if (shapeType >= 3 && shape)
	{
		const PxBounds3 bounds = PxShapeExt::getWorldBounds(*shape, *ghostActor);
		Game::Graphics::Instance().GetShapeRenderer()->DrawBox(
			Conv::ToVector3(bounds.getCenter()), Vector3::Zero,
			Conv::ToVector3(bounds.getExtents()), Color(0.1f, 0.9f, 1.0f, 1.0f));
		return;
	}

	switch (shapeType)
	{
	case 1:
	{
		Game::Graphics::Instance().GetShapeRenderer()->DrawSphere(
			Conv::ToVector3(pose.p), scaledSize.x, Color(1.0f, 0.0f, 1.0f, 1.0f));
		break;
	}

	case 2:
	{
		const Matrix capsulePose =
			Matrix::CreateRotationZ(-DirectX::XM_PIDIV2) * Conv::ToMatrix(pose);
		Game::Graphics::Instance().GetShapeRenderer()->DrawCapsule(
			capsulePose, scaledSize.x, scaledSize.y, Color(0.8f, 0.0f, 1.0f, 1.0f));
		break;
	}

	default:
	{
		const Quaternion rotation(pose.q.x, pose.q.y, pose.q.z, pose.q.w);
		Game::Graphics::Instance().GetShapeRenderer()->DrawBox(
			Conv::ToVector3(pose.p), rotation.ToEuler(), scaledSize, Color(1.0f, 0.2f, 0.0f, 1.0f));
		break;
	}
	}
}

void VMDLColliderComponent::DrawGUI()
{
	ImGui::Text("NodeIndex: %d", nodeIndex);
	ImGui::Text("Shape: %d", shapeType);
	ImGui::Text("Size: %.3f, %.3f, %.3f", size.x, size.y, size.z);
	ImGui::Text("Trigger: %s", isTrigger ? "true" : "false");
}
