// BoneCollider.cpp

#include "Physics/Collider/BoneCollider.h"
#include "Rendering/Core/RenderContext.h"
#include "Physics/RigidBody/Rigidbody.h"
#include "Rendering/Core/Graphics.h"
#include "IconsFontAwesome5.h"
#include "Gameplay/Actor/Actor.h"

BoneCollider::BoneCollider(
    Object* owner, LayerId layerId, Model* model, int nodeIndex, Matrix offset, PxMaterial* material,
    bool isTrigger, bool freezePositions, bool freezeRotations)
    : PhysicsComponent(owner, layerId), material(material), model(model), nodeIndex(nodeIndex), offset(offset),
    isTrigger(isTrigger), freezePositions{freezePositions},
    freezeRotations{freezeRotations}
{

    this->material = material ? material : PhysicsManager::Instance().GetDefaultMaterial();

    PxPhysics* physics = PhysicsManager::Instance().GetPhysics();

    _ASSERT_EXPR(model != nullptr, L"BoneCollider requires model.");
    _ASSERT_EXPR(nodeIndex >= 0, L"BoneCollider invalid nodeIndex.");
    _ASSERT_EXPR(nodeIndex < static_cast<int>(model->GetNodes().size()), L"BoneCollider nodeIndex out of range.");

    // ボーンの初期位置でKinematic Dynamicを生成
    Matrix boneWorld = model->GetNodes()[nodeIndex].worldTransform;
    Matrix world = MakeBoneOffsetWorld(boneWorld, offset);

    if (freezePositions)
    {
        world._41 = offset._41;
        world._42 = offset._42;
        world._43 = offset._43;
    }

    if (freezeRotations)
    {
        Vector3 scale, _1;
        Vector3 position;
        Quaternion rotation;
        world.Decompose(scale, rotation, position);
        offset.Decompose(_1, rotation, _1);
        world = Matrix::CreateScale(scale) * Matrix::CreateFromQuaternion(rotation) * Matrix::CreateTranslation(position);
    }

    PxTransform t = Conv::ToPxTransform(world);
    ghostActor = physics->createRigidDynamic(t);
    ghostActor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);

    ghostActor->userData = owner;
}

BoneCollider::~BoneCollider()
{
    if (ghostActor)
    {
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
}

void BoneCollider::OnEnabled()
{
    if (!ghostActor || ghostActor->getScene()) return;
    PhysicsManager::Instance().GetSceneContext().GetScene()->addActor(*ghostActor);
}

void BoneCollider::OnDisabled()
{
    if (!ghostActor) return;
    if (PxScene* scene = ghostActor->getScene()) scene->removeActor(*ghostActor);
}

void BoneCollider::InitializeShape()
{
    if (!ghostActor) return;

    if (shape)
    {
        ghostActor->detachShape(*shape);
        shape->release();
        shape = nullptr;
    }

    PxPhysics* physics = PhysicsManager::Instance().GetPhysics();

    // Triggerは攻撃判定用、Simulationは接触判定用
    shape = CreateShape(physics, material);
    shape->userData = this;
    shape->setLocalPose(GetLocalPose());
    shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, !isTrigger);
    shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, isTrigger);

    // ownerのlayerをシェイプに反映
    PhysicsManager::SetLayerToShape(shape, layerId);

    ghostActor->attachShape(*shape);

    if (IsActive() && !ghostActor->getScene())
    {
        PhysicsManager::Instance().GetSceneContext().GetScene()->addActor(*ghostActor);
    }
}

void BoneCollider::LateUpdate()
{
    UpdateShape();
}

void BoneCollider::UpdateShape()
{
    if (!ghostActor || !model || nodeIndex < 0) return;

    const auto& nodes = model->GetNodes();
    if (nodeIndex >= static_cast<int>(nodes.size())) return;

    Matrix boneWorld = nodes[nodeIndex].worldTransform;
    Matrix world = MakeBoneOffsetWorld(boneWorld, offset);

    if (freezePositions)
    {
        world._41 = offset._41;
        world._42 = offset._42;
        world._43 = offset._43;
    }

    if (freezeRotations)
    {
        Vector3 scale, _1;
        Vector3 position;
        Quaternion rotation;
        world.Decompose(scale, rotation, position);
        offset.Decompose(_1, rotation, _1);
        world = Matrix::CreateScale(scale) * Matrix::CreateFromQuaternion(rotation) * Matrix::CreateTranslation(position);
    }

    ghostActor->setKinematicTarget(Conv::ToPxTransform(world));
}

Vector3 BoneCollider::GetWorldPosition() const
{
    if (!ghostActor)
    {
        return Vector3::Zero;
    }

    const PxTransform pose = ghostActor->getGlobalPose();
    return Vector3(pose.p.x, pose.p.y, pose.p.z);
}

Actor* BoneCollider::FindOverlapActorByTag(const std::string& tag) const
{
    if (!ghostActor || !shape) return nullptr;

    PxGeometryHolder geometry = shape->getGeometry();
    PxTransform pose = PxShapeExt::getGlobalPose(*shape, *ghostActor);
    PxOverlapBuffer hit;
    bool hitAny = PhysicsManager::Instance()
        .GetSceneContext()
        .GetScene()
        ->overlap(geometry.any(), pose, hit);
    if (!hitAny) return nullptr;

    for (PxU32 i = 0; i < hit.getNbAnyHits(); ++i)
    {
        PxShape* hitShape = hit.getAnyHit(i).shape;
        if (!hitShape) continue;
        if (hitShape == shape) continue;

        PhysicsComponent* collider = static_cast<PhysicsComponent*>(hitShape->userData);
        if (!collider) continue;

        Actor* actor = dynamic_cast<Actor*>(collider->GetOwner());
        if (!actor) continue;
        if (actor == dynamic_cast<Actor*>(owner)) continue;
        if (actor->CompareTag(tag)) return actor;
    }

    return nullptr;
}

void BoneCollider::DrawBoneSettingsGUI()
{
    ImGui::Text("NodeIndex: %d", nodeIndex);

    Vector3 pos;
    Quaternion rot;
    Vector3 scale;
    Vector3 euler;
    offset.Decompose(scale, rot, pos);
    if (ImGui::DragFloat3("OffsetPos", &pos.x, 0.01f))
    {
        offset = Matrix::CreateScale(scale) *
            Matrix::CreateFromQuaternion(rot) *
            Matrix::CreateTranslation(pos);
    }
    euler = rot.ToEuler();
    euler.x = DEG(euler.x);
    euler.y = DEG(euler.y);
    euler.z = DEG(euler.z);
    if (ImGui::DragFloat3("OffsetRotation", &euler.x, 0.01f))
    {
        offset = Matrix::CreateScale(scale) *
            Matrix::CreateFromYawPitchRoll(RAD(euler.y), RAD(euler.x), RAD(euler.z)) *
            Matrix::CreateTranslation(pos);
    }
    ImGui::Checkbox("Freeze Positions", &freezePositions);
    ImGui::SameLine();
    ImGui::Checkbox("Freeze Rotations", &freezeRotations);
}