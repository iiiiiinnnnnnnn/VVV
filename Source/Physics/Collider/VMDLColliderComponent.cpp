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
}

VMDLColliderComponent::VMDLColliderComponent(
    Object* owner,
    LayerId layerId,
    VMDLModel* model,
    int nodeIndex,
    int shapeType,
    const Vector3& size,
    const Matrix& offset,
    PxMaterial* material,
    bool isTrigger)
    : PhysicsComponent(owner, layerId)
    , model(model)
    , nodeIndex(nodeIndex)
    , shapeType(shapeType)
    , size(size)
    , offset(offset)
    , isTrigger(isTrigger)
    , material(material ? material : PhysicsManager::Instance().GetDefaultMaterial())
{
    _ASSERT_EXPR(model != nullptr, L"requires model.");
    _ASSERT_EXPR(nodeIndex >= 0, L"invalid nodeIndex.");
    _ASSERT_EXPR(
        model && nodeIndex < static_cast<int>(model->GetNodes().size()),
        L"nodeIndex out of range.");

    if (!model || nodeIndex < 0 || nodeIndex >= static_cast<int>(model->GetNodes().size()))
    {
        return;
    }

    PxPhysics* physics = PhysicsManager::Instance().GetPhysics();
    const Matrix world = MakeColliderWorld(*model, model->GetNodes()[nodeIndex].worldTransform, offset);

    ghostActor = physics->createRigidDynamic(Conv::ToPxTransform(world));
    ghostActor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
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

    switch (shapeType)
    {
    case 1:
        shape = physics->createShape(
            PxSphereGeometry(std::max(0.001f, scaledSize.x)),
            *material,
            true);
        break;

    case 2:
        shape = physics->createShape(
            PxCapsuleGeometry(
                std::max(0.001f, scaledSize.x),
                std::max(0.001f, scaledSize.y) * 0.5f),
            *material,
            true);
        break;

    default:
        shape = physics->createShape(
            PxBoxGeometry(
                std::max(0.001f, scaledSize.x),
                std::max(0.001f, scaledSize.y),
                std::max(0.001f, scaledSize.z)),
            *material,
            true);
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

void VMDLColliderComponent::UpdateScaledSize(const Matrix&)
{
    scaledSize = model ? model->GetScaledAttachmentVector(size) : size;
}

PxTransform VMDLColliderComponent::GetShapeLocalPose() const
{
    if (shapeType == 2)
    {
        return PxTransform(
            PxVec3(0.0f, 0.0f, 0.0f),
            PxQuat(DirectX::XM_PIDIV2, PxVec3(0.0f, 0.0f, 1.0f)));
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
    const Vector3 previousScaledSize = scaledSize;
    UpdateScaledSize(world);
    if ((scaledSize - previousScaledSize).LengthSquared() > 0.000001f) CreateShape();
    ghostActor->setGlobalPose(Conv::ToPxTransform(world), false);
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

    return Conv::ToVector3(
        PxShapeExt::getGlobalPose(*shape, *ghostActor).p);
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

    const bool hitAny = PhysicsManager::Instance()
        .GetSceneContext()
        .GetScene()
        ->overlap(geometry.any(), pose, hit);

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

    const PxTransform pose = shape
        ? PxShapeExt::getGlobalPose(*shape, *ghostActor)
        : ghostActor->getGlobalPose();

    switch (shapeType)
    {
        case 1:
        {
            Game::Graphics::Instance().GetShapeRenderer()->DrawSphere(
                Conv::ToVector3(pose.p),
                scaledSize.x,
                Color(1.0f, 0.0f, 1.0f, 1.0f));
            break;
        }

        case 2:
        {
            const Matrix capsulePose =
                Matrix::CreateRotationZ(-DirectX::XM_PIDIV2) *
                Conv::ToMatrix(pose);
            Game::Graphics::Instance().GetShapeRenderer()->DrawCapsule(
                capsulePose,
                scaledSize.x,
                scaledSize.y,
                Color(0.8f, 0.0f, 1.0f, 1.0f));
            break;
        }

        default:
        {
            const Quaternion rotation(pose.q.x, pose.q.y, pose.q.z, pose.q.w);
            Game::Graphics::Instance().GetShapeRenderer()->DrawBox(
                Conv::ToVector3(pose.p),
                rotation.ToEuler(),
                scaledSize,
                Color(1.0f, 0.2f, 0.0f, 1.0f));
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
