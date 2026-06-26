// Collider.cpp

#include "Collider.h"
#include "Actor.h"
#include "Graphics.h"
#include "ShapeRenderer.h"
#include "Terrain.h"
#include "IconsFontAwesome5.h"

#include <cmath>
#include <cfloat>

namespace
{
    struct TerrainColliderVxHeader
    {
        char magic[4] = {'V', 'V', 'V', 'X'};
        uint32_t version = 1;
        uint32_t vertexCount = 0;
        uint32_t indexCount = 0;
        float minX = 0.0f;
        float maxX = 1.0f;
        float minZ = 0.0f;
        float maxZ = 1.0f;
        float scaleX = 1.0f;
        float scaleY = 1.0f;
        float scaleZ = 1.0f;
        float terrainSize = 500.0f;
        int gridResolution = 0;
        float edgeFactor = 0.0f;
        float innerFactor = 0.0f;
        float heightScaler = 0.0f;
    };

    bool NearlyEqual(float a, float b)
    {
        return std::fabs(a - b) <= 0.0001f;
    }
}

static bool ShouldRenderColliderDebug(const Collider* collider)
{
    return collider && collider->ShouldRenderDebug();
}

BoxCollider::BoxCollider(Object* owner, Rigidbody* rigidbody, const Vector3& size, PxMaterial* material)
    : BoxCollider(owner, rigidbody, size, Vector3::Zero, material)
{
}

BoxCollider::BoxCollider(
    Object* owner,
    Rigidbody* rigidbody,
    const Vector3& size,
    const Vector3& localPosition,
    PxMaterial* material)
    : Collider(owner)
    , rigidbody(rigidbody)
    , material(material)
    , size(size)
    , localPosition(localPosition)
{
    // エラー用
    Component::GetOwnerAsActor();

    this->material = material ? material : PhysicsManager::Instance().GetDefaultMaterial();
    UpdateShape();
}

void BoxCollider::Render(const RenderContext& rc)
{
    if (!ShouldRenderColliderDebug(this)) return;
    if (!rc.renderSettings.showDebug) return;

    PxTransform pose =
        rigidbody->GetRigidActor()->getGlobalPose() *
        MakeLocalPose();

    Quaternion rotation(pose.q.x, pose.q.y, pose.q.z, pose.q.w);

    Game::Graphics::Instance().GetShapeRenderer()->DrawBox(
        Conv::ToVector3(pose.p), rotation.ToEuler(), size, {0.0f, 1.0f, 0.0f, 1.0f});
}

PxTransform BoxCollider::MakeLocalPose() const
{
    return PxTransform(
        PxVec3(localPosition.x, localPosition.y, localPosition.z));
}

void BoxCollider::UpdateShape()
{
    PxPhysics* physics = PhysicsManager::Instance().GetPhysics();
    PxRigidActor* rigidActor = rigidbody->GetRigidActor();

    // 古いシェイプを削除
    if (shape)
    {
        rigidActor->detachShape(*shape);
        shape->release();
        shape = nullptr;
    }

    // 新しいシェイプを生成
    shape = physics->createShape(
        PxBoxGeometry(size.x, size.y, size.z), *material);
    shape->userData = this;
    shape->setLocalPose(MakeLocalPose());

    // ownerのlayerをシェイプに反映
    Actor* actor = Component::GetOwnerAsActor();
    PhysicsManager::SetLayerToShape(shape, actor->GetLayer());
	rigidActor->attachShape(*shape);
}

void BoxCollider::DrawGUI()
{
    isOpenGUI = ImGui::TreeNode(ICON_FA_SHAPES " BoxCollider");
    if (isOpenGUI)
    {
        bool changed = false;
        changed |= ImGui::DragFloat3("Size", &size.x, 0.01f, 0.01f, 100.0f);
        changed |= ImGui::DragFloat3("Local Position", &localPosition.x, 0.01f);

        if (changed) UpdateShape();

        if (ImGui::TreeNode(ICON_FA_GRIP_LINES " Material"))
        {
            float sfriction = material->getStaticFriction();
            float dfriction = material->getDynamicFriction();
            float restitution = material->getRestitution();
            if (ImGui::DragFloat("Static Friction", &sfriction, 0.01f, 0.0f, 1.0f))
                material->setStaticFriction(sfriction);
            if (ImGui::DragFloat("Dynamic Friction", &dfriction, 0.01f, 0.0f, 1.0f))
                material->setDynamicFriction(dfriction);
            if (ImGui::DragFloat("Restitution", &restitution, 0.01f, 0.0f, 1.0f))
                material->setRestitution(restitution);
            ImGui::TreePop();
        }

        ImGui::TreePop();
    }
}

CapsuleCollider::CapsuleCollider(Object* owner, Rigidbody* rigidbody, float radius, float height, PxMaterial* material)
    : CapsuleCollider(
        owner,
        rigidbody,
        radius,
        height,
        Vector3::Zero,
        material)
{
}

CapsuleCollider::CapsuleCollider(
    Object* owner,
    Rigidbody* rigidbody,
    float radius,
    float height,
    const Vector3& localPosition,
    PxMaterial* material)
    : Collider(owner)
    , rigidbody(rigidbody)
    , material(material)
    , radius(radius)
    , height(height)
    , localPosition(localPosition)
{
	// エラー用
    Component::GetOwnerAsActor();

    this->material = material ? material : PhysicsManager::Instance().GetDefaultMaterial();
    UpdateShape();
}

PxTransform CapsuleCollider::MakeLocalPose() const
{
    return PxTransform(
        PxVec3(localPosition.x, localPosition.y, localPosition.z),
        PxQuat(DirectX::XM_PIDIV2, PxVec3(0, 0, 1))
    );
}

void CapsuleCollider::Render(const RenderContext& rc)
{
    if (!ShouldRenderColliderDebug(this)) return;
    if (!rc.renderSettings.showDebug) return;

    PxTransform pose =
        rigidbody->GetRigidActor()->getGlobalPose() *
        MakeLocalPose();

    Game::Graphics::Instance().GetShapeRenderer()->DrawCapsule(
        Conv::ToMatrix(pose),
        radius,
        height,
        {0.0f, 1.0f, 0.0f, 1.0f});
}

void CapsuleCollider::UpdateShape()
{
    PxPhysics* physics = PhysicsManager::Instance().GetPhysics();
    PxRigidActor* rigidActor = rigidbody->GetRigidActor();

    // 古いシェイプを削除
    if (shape)
    {
        rigidActor->detachShape(*shape);
        shape->release();
        shape = nullptr;
    }

    // 新しいシェイプを生成
    shape = physics->createShape(
        PxCapsuleGeometry(radius, height * 0.5f), *material);
    shape->userData = this;
    shape->setLocalPose(MakeLocalPose());

    // ownerのlayerをシェイプに反映
    Actor* actor = Component::GetOwnerAsActor();
    PhysicsManager::SetLayerToShape(shape, actor->GetLayer());
    rigidActor->attachShape(*shape);
}

void CapsuleCollider::DrawGUI()
{
	isOpenGUI = ImGui::TreeNode(ICON_FA_SHAPES " CapsuleCollider");
    if (isOpenGUI)
    {
        bool changed = false;
        changed |= ImGui::DragFloat("Radius", &radius, 0.01f, 0.01f, 100.0f);
        changed |= ImGui::DragFloat("Height", &height, 0.01f, 0.01f, 100.0f);
        changed |= ImGui::DragFloat3("Local Position", &localPosition.x, 0.01f);
		if (radius < 0.01f) radius = 0.01f;

        if (changed) UpdateShape();

        if (ImGui::TreeNode(ICON_FA_GRIP_LINES " Material")) {
            float sfriction = material->getStaticFriction();
            float dfriction = material->getDynamicFriction();
            float restitution = material->getRestitution();
            if (ImGui::DragFloat("Static Friction", &sfriction, 0.01f, 0.0f, 1.0f))
                material->setStaticFriction(sfriction);
            if (ImGui::DragFloat("Dynamic Friction", &dfriction, 0.01f, 0.0f, 1.0f))
                material->setDynamicFriction(dfriction);
            if (ImGui::DragFloat("Restitution", &restitution, 0.01f, 0.0f, 1.0f))
                material->setRestitution(restitution);
            ImGui::TreePop();
        }

        ImGui::TreePop();
    }
}

SphereCollider::SphereCollider(Object* owner, Rigidbody* rigidbody, float radius, PxMaterial* material)
    : SphereCollider(owner, rigidbody, radius, Vector3(0, radius, 0), material)
{
}

SphereCollider::SphereCollider(
    Object* owner,
    Rigidbody* rigidbody,
    float radius,
    const Vector3& localPosition,
    PxMaterial* material)
    : Collider(owner)
    , rigidbody(rigidbody)
    , material(material)
    , radius(radius)
    , localPosition(localPosition)
{
	// エラー用
    Component::GetOwnerAsActor();

    this->material = material ? material : PhysicsManager::Instance().GetDefaultMaterial();
    UpdateShape();
}

PxTransform SphereCollider::MakeLocalPose() const
{
    return PxTransform(PxVec3(localPosition.x, localPosition.y, localPosition.z));
}

void SphereCollider::Render(const RenderContext& rc)
{
    if (!ShouldRenderColliderDebug(this)) return;
	if (!rc.renderSettings.showDebug) return;

    PxTransform pose =
        rigidbody->GetRigidActor()->getGlobalPose() *
        MakeLocalPose();

    Game::Graphics::Instance().GetShapeRenderer()->DrawSphere(
        Conv::ToVector3(pose.p), radius, { 0.0f, 1.0f, 0.0f, 1.0f });
}

void SphereCollider::UpdateShape()
{
    PxPhysics* physics = PhysicsManager::Instance().GetPhysics();
    PxRigidActor* rigidActor = rigidbody->GetRigidActor();

    // 古いシェイプを削除
    if (shape)
    {
        rigidActor->detachShape(*shape);
        shape->release();
        shape = nullptr;
    }

    // 新しいシェイプを生成
    shape = physics->createShape(
        PxSphereGeometry(radius), *material);
    shape->userData = this;
    shape->setLocalPose(MakeLocalPose());

    // ownerのlayerをシェイプに反映
    Actor* actor = Component::GetOwnerAsActor();
    PhysicsManager::SetLayerToShape(shape, actor->GetLayer());
    rigidActor->attachShape(*shape);
}

void SphereCollider::DrawGUI()
{
	isOpenGUI = ImGui::TreeNode(ICON_FA_SHAPES " SphereCollider");
    if (isOpenGUI)
    {
        bool changed = false;
        changed |= ImGui::DragFloat("Radius", &radius, 0.01f, 0.01f, 100.0f);
        changed |= ImGui::DragFloat3("Local Position", &localPosition.x, 0.01f);
        if (radius < 0.01f) radius = 0.01f;

        if (changed) UpdateShape();

        if (ImGui::TreeNode(ICON_FA_GRIP_LINES " Material")) {
            float sfriction = material->getStaticFriction();
            float dfriction = material->getDynamicFriction();
            float restitution = material->getRestitution();
            if (ImGui::DragFloat("Static Friction", &sfriction, 0.01f, 0.0f, 1.0f))
                material->setStaticFriction(sfriction);
            if (ImGui::DragFloat("Dynamic Friction", &dfriction, 0.01f, 0.0f, 1.0f))
                material->setDynamicFriction(dfriction);
            if (ImGui::DragFloat("Restitution", &restitution, 0.01f, 0.0f, 1.0f))
                material->setRestitution(restitution);
            ImGui::TreePop();
        }

        ImGui::TreePop();
    }
}

void MeshCollider::Render(const RenderContext& rc)
{
    if (!ShouldRenderColliderDebug(this)) return;
    if (!rc.renderSettings.showDebug) return;
    if (!model) return;

    Vector3 minPosition(FLT_MAX, FLT_MAX, FLT_MAX);
    Vector3 maxPosition(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    bool hasVertex = false;

    for (const Model::Mesh& mesh : model->GetMeshes())
    {
        if (!mesh.isDraw) continue;
        if (!mesh.node) continue;

        for (const Model::Vertex& vertex : mesh.vertices)
        {
            Vector3 position =
                Vector3::Transform(
                    vertex.position,
                    mesh.node->worldTransform);

            if (position.x < minPosition.x) minPosition.x = position.x;
            if (position.y < minPosition.y) minPosition.y = position.y;
            if (position.z < minPosition.z) minPosition.z = position.z;

            if (position.x > maxPosition.x) maxPosition.x = position.x;
            if (position.y > maxPosition.y) maxPosition.y = position.y;
            if (position.z > maxPosition.z) maxPosition.z = position.z;

            hasVertex = true;
        }
    }

    if (!hasVertex) return;

    Game::Graphics::Instance().GetShapeRenderer()->DrawBox(
        (minPosition + maxPosition) * 0.5f,
        Vector3::Zero,
        (maxPosition - minPosition) * 0.5f,
        Color(0.0f, 1.0f, 1.0f, 1.0f));
}

MeshCollider::MeshCollider(Object* owner, Rigidbody* rigidbody, Model* model, PxMaterial* material)
    : Collider(owner), rigidbody(rigidbody), model(model), useConvex(false), quantizedCount(32), material(material)
{
    this->material = material ? material : PhysicsManager::Instance().GetDefaultMaterial();
    UpdateShape();
}

MeshCollider::MeshCollider(Object* owner, Rigidbody* rigidbody, Model* model, bool useConvex, unsigned int quantizedCount, PxMaterial* material)
    : Collider(owner), rigidbody(rigidbody), model(model), useConvex(useConvex), quantizedCount(quantizedCount), material(material)
{
    this->material = material ? material : PhysicsManager::Instance().GetDefaultMaterial();
    UpdateShape();
}

void MeshCollider::UpdateShape()
{
    PxPhysics* physics = PhysicsManager::Instance().GetPhysics();
    PxCookingParams* cookingParams = PhysicsManager::Instance().GetCooking();
    PxRigidActor* rigidActor = rigidbody->GetRigidActor();

    // 既存シェイプを全部外す
    PxU32 shapeCount = rigidActor->getNbShapes();
    std::vector<PxShape*> shapes(shapeCount);
    rigidActor->getShapes(shapes.data(), shapeCount);
    for (PxShape* s : shapes)
        rigidActor->detachShape(*s);

    for (const Model::Mesh& mesh : model->GetMeshes())
    {
        if (!mesh.isDraw) continue;

        std::vector<PxVec3> vertices;
        for (const Model::Vertex& v : mesh.vertices)
        {
            Vector3 pos = Vector3::Transform(v.position, mesh.node->worldTransform);
            vertices.push_back(PxVec3(pos.x, pos.y, pos.z));
        }

        PxShape* shape = nullptr;

        if (useConvex)
        {
            if (vertices.size() < 4)
            {
                OutputDebugStringA("MeshCollider: skipped convex mesh with fewer than 4 vertices.\n");
                continue;
            }

            PxConvexMeshDesc convexDesc;
            convexDesc.points.count  = (PxU32)vertices.size();
            convexDesc.points.stride = sizeof(PxVec3);
            convexDesc.points.data   = vertices.data();
            convexDesc.flags         = PxConvexFlag::eCOMPUTE_CONVEX | PxConvexFlag::eQUANTIZE_INPUT;
            convexDesc.quantizedCount = quantizedCount;

            PxDefaultMemoryOutputStream writeBuffer;
            const bool cooked = PxCookConvexMesh(*cookingParams, convexDesc, writeBuffer);
            if (!cooked || writeBuffer.getSize() == 0)
            {
                OutputDebugStringA("MeshCollider: skipped convex mesh because PhysX cooking failed.\n");
                continue;
            }

            PxDefaultMemoryInputData readBuffer(writeBuffer.getData(), writeBuffer.getSize());
            PxConvexMesh* convexMesh = physics->createConvexMesh(readBuffer);
            if (convexMesh == nullptr)
            {
                OutputDebugStringA("MeshCollider: skipped convex mesh because createConvexMesh failed.\n");
                continue;
            }

            shape = physics->createShape(PxConvexMeshGeometry(convexMesh), *material);
            convexMesh->release();
        }
        else
        {
            PxTriangleMeshDesc meshDesc;
            meshDesc.points.count     = (PxU32)vertices.size();
            meshDesc.points.stride    = sizeof(PxVec3);
            meshDesc.points.data      = vertices.data();
            meshDesc.triangles.count  = (PxU32)(mesh.indices.size() / 3);
            meshDesc.triangles.stride = 3 * sizeof(PxU32);
            meshDesc.triangles.data   = mesh.indices.data();

            PxDefaultMemoryOutputStream writeBuffer;
            const bool cooked = PxCookTriangleMesh(*cookingParams, meshDesc, writeBuffer);
            if (!cooked || writeBuffer.getSize() == 0)
            {
                OutputDebugStringA("MeshCollider: skipped triangle mesh because PhysX cooking failed.\n");
                continue;
            }

            PxDefaultMemoryInputData readBuffer(writeBuffer.getData(), writeBuffer.getSize());
            PxTriangleMesh* triangleMesh = physics->createTriangleMesh(readBuffer);
            if (triangleMesh == nullptr)
            {
                OutputDebugStringA("MeshCollider: skipped triangle mesh because createTriangleMesh failed.\n");
                continue;
            }

            shape = physics->createShape(PxTriangleMeshGeometry(triangleMesh), *material);
            triangleMesh->release();
        }

        if (shape == nullptr)
        {
            OutputDebugStringA("MeshCollider: skipped mesh because createShape failed.\n");
            continue;
        }

        shape->userData = this;

        // ownerのlayerをシェイプに反映
        Actor* actor = Component::GetOwnerAsActor();
        PhysicsManager::SetLayerToShape(shape, actor->GetLayer());
        rigidActor->attachShape(*shape);

        shape->release();
    }
}

void MeshCollider::DrawGUI()
{
	isOpenGUI = ImGui::TreeNode(ICON_FA_SHAPES " MeshCollider");
    if (isOpenGUI)
    {
        if (useConvex)
        {
            ImGui::Text("ConvexMesh");
			ImGui::Text("Quantized Count: %u", quantizedCount);
        }
        else
        {
            ImGui::Text("TriangleMesh");
        }
        ImGui::TreePop();
    }
}

static Matrix MakeBoneOffsetWorld(const Matrix& boneWorld, const Matrix& offset)
{
    Vector3 scale;
    Vector3 position;
    Quaternion rotation;
    Matrix boneWorldCopy = boneWorld;
    boneWorldCopy.Decompose(scale, rotation, position);

    Matrix boneTransform =
        Matrix::CreateScale(scale) *
        Matrix::CreateFromQuaternion(rotation) *
        Matrix::CreateTranslation(position);

    return offset * boneTransform;
}

BoneSphereCollider::BoneSphereCollider(Object* owner, Model* model,
    int nodeIndex, float radius, Matrix offset, PxMaterial* material,
    bool isTrigger, bool freezePositions, bool freezeRotations)
    : BoneCollider(owner, model, nodeIndex, offset, material, isTrigger, freezePositions, freezeRotations)
    , radius(radius)
{
    InitializeShape();
}

BoneCollider::BoneCollider(
    Object* owner, Model* model, int nodeIndex, Matrix offset, PxMaterial* material,
    bool isTrigger, bool freezePositions, bool freezeRotations)
    : Collider(owner), material(material), model(model), nodeIndex(nodeIndex), offset(offset),
	isTrigger(isTrigger), freezePositions{freezePositions},
    freezeRotations{freezeRotations}
{
    Component::GetOwnerAsActor();

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
    Actor* actor = Component::GetOwnerAsActor();

    // Triggerは攻撃判定用、Simulationは接触判定用
    shape = CreateShape(physics, material);
    shape->userData = this;
    shape->setLocalPose(GetLocalPose());
    shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, !isTrigger);
    shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, isTrigger);

    // ownerのlayerをシェイプに反映
    PhysicsManager::SetLayerToShape(shape, actor->GetLayer());

    ghostActor->attachShape(*shape);

    if (!ghostActor->getScene())
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

        Collider* collider = static_cast<Collider*>(hitShape->userData);
        if (!collider) continue;

        Actor* actor = collider->GetOwnerActor();
        if (!actor) continue;
        if (actor == GetOwnerActor()) continue;
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

PxShape* BoneSphereCollider::CreateShape(PxPhysics* physics, PxMaterial* material) const
{
    return physics->createShape(PxSphereGeometry(radius), *material, true);
}

void BoneSphereCollider::Render(const RenderContext& rc)
{
    if (!ShouldRenderColliderDebug(this)) return;
    if (!rc.renderSettings.showDebug || !ghostActor) return;

    PxTransform t = ghostActor->getGlobalPose();
    Matrix m = Conv::ToMatrix(t);
    Vector3 pos(m._41, m._42, m._43);

    Game::Graphics::Instance().GetShapeRenderer()->DrawSphere(
        pos, radius, Color(1.0f, 0.0f, 1.0f, 1.0f));
}

void BoneSphereCollider::DrawGUI()
{
	isOpenGUI = ImGui::TreeNode(ICON_FA_SHAPES " BoneSphereCollider");
    if (isOpenGUI)
    {
        bool changed = false;
        changed |= ImGui::DragFloat("Radius", &radius, 0.01f, 0.01f, 10.0f);
        if (radius < 0.01f) radius = 0.01f;
        if (changed)
        {
            InitializeShape();
        }
        DrawBoneSettingsGUI();
        ImGui::TreePop();
    }
}

BoneCapsuleCollider::BoneCapsuleCollider(Object* owner, Model* model, int nodeIndex, float radius, float height, Matrix offset, PxMaterial* material, bool isTrigger, bool freezePositions, bool freezeRotations)
    : BoneCollider(owner, model, nodeIndex, offset, material, isTrigger, freezePositions, freezeRotations)
    , radius(radius), height(height)
{
    InitializeShape();
}

PxShape* BoneCapsuleCollider::CreateShape(PxPhysics* physics, PxMaterial* material) const
{
    return physics->createShape(
        PxCapsuleGeometry(radius, height * 0.5f),
        *material,
        true);
}

PxTransform BoneCapsuleCollider::GetLocalPose() const
{
    return PxTransform(
        PxVec3(0, 0, 0),
        PxQuat(DirectX::XM_PIDIV2, PxVec3(0, 0, 1)));
}

void BoneCapsuleCollider::Render(const RenderContext& rc)
{
    if (!ShouldRenderColliderDebug(this)) return;
    if (!rc.renderSettings.showDebug || !ghostActor) return;

    PxTransform pose = ghostActor->getGlobalPose() *
        GetLocalPose() *
        PxTransform(PxVec3(0, 0, 0), PxQuat(-DirectX::XM_PIDIV2, PxVec3(0, 0, 1)));

    Game::Graphics::Instance().GetShapeRenderer()->DrawCapsule(
        Conv::ToMatrix(pose),
        radius,
        height,
        Color(0.8f, 0.0f, 1.0f, 1.0f));
}

void BoneCapsuleCollider::DrawGUI()
{
	isOpenGUI = ImGui::TreeNode(ICON_FA_SHAPES " BoneCapsuleCollider");
    if (isOpenGUI)
    {
        bool changed = false;
        changed |= ImGui::DragFloat("Radius", &radius, 0.01f, 0.01f, 10.0f);
        changed |= ImGui::DragFloat("Height", &height, 0.01f, 0.01f, 10.0f);
        radius = max(radius, 0.01f);
        height = max(height, 0.01f);
        if (changed)
        {
            InitializeShape();
        }
        DrawBoneSettingsGUI();
        ImGui::TreePop();
    }
}

BoneBoxCollider::BoneBoxCollider(Object* owner, Model* model, int nodeIndex,
    const Vector3& size, Matrix offset, PxMaterial* material,
    bool isTrigger, bool freezePositions, bool freezeRotations)
    : BoneCollider(owner, model, nodeIndex, offset, material, isTrigger, freezePositions, freezeRotations)
    , size(size)
{
    InitializeShape();
}

PxShape* BoneBoxCollider::CreateShape(PxPhysics* physics, PxMaterial* material) const
{
    return physics->createShape(
        PxBoxGeometry(size.x, size.y, size.z),
        *material,
        true);
}

void BoneBoxCollider::Render(const RenderContext& rc)
{
    if (!ShouldRenderColliderDebug(this)) return;
    if (!rc.renderSettings.showDebug || !ghostActor) return;

    PxTransform t = ghostActor->getGlobalPose();
    Quaternion rotation(t.q.x, t.q.y, t.q.z, t.q.w);
    Game::Graphics::Instance().GetShapeRenderer()->DrawBox(
        Conv::ToVector3(t.p),
        rotation.ToEuler(),
        size,
        Color(1.0f, 0.2f, 0.0f, 1.0f));
}

void BoneBoxCollider::DrawGUI()
{
	isOpenGUI = ImGui::TreeNode(ICON_FA_SHAPES " BoneBoxCollider");
    if (isOpenGUI)
    {
        bool changed = false;
        changed |= ImGui::DragFloat3("Size", &size.x, 0.01f, 0.01f, 10.0f);
        size.x = max(size.x, 0.01f);
        size.y = max(size.y, 0.01f);
        size.z = max(size.z, 0.01f);
        if (changed)
        {
            InitializeShape();
        }
        DrawBoneSettingsGUI();
        ImGui::TreePop();
    }
}

TerrainMeshCollider::TerrainMeshCollider(Object* owner, Rigidbody* rigidbody, const CollisionArea& collisionArea, PxMaterial* material)
    : Collider(owner), rigidbody(rigidbody), collisionArea(collisionArea), material(material)
{
    GetOwnerAsActor();

    _ASSERT_EXPR(rigidbody != nullptr, L"TerrainMeshCollider requires Rigidbody.");

    this->material = material ? material : PhysicsManager::Instance().GetDefaultMaterial();

    std::vector<Vector3> vertices;
    std::vector<uint32_t> indices;
    if (LoadCachedMesh(vertices, indices))
    {
        UpdateShape(vertices, indices);
        debugVertices = vertices;
        debugIndices = indices;
    }
    else
    {
        pendingGpuRebuild = true;
    }
}

TerrainMeshCollider::~TerrainMeshCollider()
{
    if (shape != nullptr)
    {
        shape->release();
        shape = nullptr;
    }
}

void TerrainMeshCollider::ReleaseShape()
{
    if (shape == nullptr)
    {
        return;
    }

    if (rigidbody != nullptr && rigidbody->GetRigidActor() != nullptr)
    {
        rigidbody->GetRigidActor()->detachShape(*shape);
    }

    shape->release();
    shape = nullptr;
}

void TerrainMeshCollider::ClampCollisionArea()
{
    collisionArea.minX = std::clamp(collisionArea.minX, 0.0f, 1.0f);
    collisionArea.minZ = std::clamp(collisionArea.minZ, 0.0f, 1.0f);
    collisionArea.maxX = std::clamp(collisionArea.maxX, 0.0f, 1.0f);
    collisionArea.maxZ = std::clamp(collisionArea.maxZ, 0.0f, 1.0f);

    if (collisionArea.minX > collisionArea.maxX)
    {
        std::swap(collisionArea.minX, collisionArea.maxX);
    }

    if (collisionArea.minZ > collisionArea.maxZ)
    {
        std::swap(collisionArea.minZ, collisionArea.maxZ);
    }

    constexpr float minRange = 0.001f;
    if (collisionArea.maxX - collisionArea.minX < minRange)
    {
        collisionArea.maxX = min(collisionArea.minX + minRange, 1.0f);
        collisionArea.minX = max(collisionArea.maxX - minRange, 0.0f);
    }

    if (collisionArea.maxZ - collisionArea.minZ < minRange)
    {
        collisionArea.maxZ = min(collisionArea.minZ + minRange, 1.0f);
        collisionArea.minZ = max(collisionArea.maxZ - minRange, 0.0f);
    }
}

void TerrainMeshCollider::RebuildFromTerrain()
{
    std::vector<Vector3> vertices;
    std::vector<uint32_t> indices;
    Terrain* terrain = owner->GetComponent<Terrain>();
    _ASSERT_EXPR(terrain != nullptr, L"TerrainMeshCollider requires Terrain component.");

    ClampCollisionArea();
    if (!terrain->BuildGpuColliderMesh(
        collisionArea.minX,
        collisionArea.maxX,
        collisionArea.minZ,
        collisionArea.maxZ,
        vertices,
        indices))
    {
        vxMessage = "GPU collider bake failed.";
        return;
    }

    ApplyOwnerScale(vertices);
    UpdateShape(vertices, indices);
    SaveCachedMesh(vertices, indices);

    debugVertices = vertices;
    debugIndices = indices;
    pendingGpuRebuild = false;
}

bool TerrainMeshCollider::LoadCachedMesh(
    std::vector<Vector3>& vertices,
    std::vector<uint32_t>& indices)
{
    Terrain* terrain = owner->GetComponent<Terrain>();
    if (!terrain)
    {
        vxMessage = "Terrain .vx load skipped: Terrain is missing.";
        return false;
    }

    ClampCollisionArea();
    const std::filesystem::path filepath = terrain->GetColliderVertexPath();
    if (!std::filesystem::exists(filepath))
    {
        vxMessage = "Terrain .vx not found. GPU bake will create it.";
        return false;
    }

    std::ifstream file(filepath, std::ios::binary);
    if (!file)
    {
        vxMessage = "Terrain .vx load failed: open failed.";
        return false;
    }

    TerrainColliderVxHeader header{};
    file.read(reinterpret_cast<char*>(&header), sizeof(header));

    if (!file ||
        header.magic[0] != 'V' ||
        header.magic[1] != 'V' ||
        header.magic[2] != 'V' ||
        header.magic[3] != 'X' ||
        header.version != 1 ||
        header.vertexCount == 0 ||
        header.indexCount == 0)
    {
        vxMessage = "Terrain .vx load failed: invalid header.";
        return false;
    }

    Actor* actor = GetOwnerAsActor();
    const Vector3 scale = actor->transform.scale;
    if (!NearlyEqual(header.minX, collisionArea.minX) ||
        !NearlyEqual(header.maxX, collisionArea.maxX) ||
        !NearlyEqual(header.minZ, collisionArea.minZ) ||
        !NearlyEqual(header.maxZ, collisionArea.maxZ) ||
        !NearlyEqual(header.scaleX, scale.x) ||
        !NearlyEqual(header.scaleY, scale.y) ||
        !NearlyEqual(header.scaleZ, scale.z) ||
        !NearlyEqual(header.terrainSize, terrain->GetTerrainSize()) ||
        header.gridResolution != terrain->GetGridResolution() ||
        !NearlyEqual(header.edgeFactor, terrain->GetTessellationEdgeFactor()) ||
        !NearlyEqual(header.innerFactor, terrain->GetTessellationInnerFactor()) ||
        !NearlyEqual(header.heightScaler, terrain->GetHeightScaler()))
    {
        vxMessage = "Terrain .vx ignored: terrain collider setting changed.";
        return false;
    }

    vertices.resize(header.vertexCount);
    indices.resize(header.indexCount);

    file.read(
        reinterpret_cast<char*>(vertices.data()),
        sizeof(Vector3) * vertices.size());
    file.read(
        reinterpret_cast<char*>(indices.data()),
        sizeof(uint32_t) * indices.size());

    if (!file)
    {
        vertices.clear();
        indices.clear();
        vxMessage = "Terrain .vx load failed: data is truncated.";
        return false;
    }

    vxMessage = "Terrain .vx loaded: " + filepath.generic_string();
    return true;
}

bool TerrainMeshCollider::SaveCachedMesh(
    const std::vector<Vector3>& vertices,
    const std::vector<uint32_t>& indices)
{
    Terrain* terrain = owner->GetComponent<Terrain>();
    if (!terrain || vertices.empty() || indices.empty())
    {
        vxMessage = "Terrain .vx save skipped: mesh is empty.";
        return false;
    }

    const std::filesystem::path filepath = terrain->GetColliderVertexPath();
    std::error_code error;
    if (!filepath.parent_path().empty())
    {
        std::filesystem::create_directories(filepath.parent_path(), error);
    }

    if (error)
    {
        vxMessage = "Terrain .vx save failed: directory creation failed.";
        return false;
    }

    std::ofstream file(filepath, std::ios::binary);
    if (!file)
    {
        vxMessage = "Terrain .vx save failed: open failed.";
        return false;
    }

    Actor* actor = GetOwnerAsActor();
    const Vector3 scale = actor->transform.scale;

    TerrainColliderVxHeader header{};
    header.vertexCount = static_cast<uint32_t>(vertices.size());
    header.indexCount = static_cast<uint32_t>(indices.size());
    header.minX = collisionArea.minX;
    header.maxX = collisionArea.maxX;
    header.minZ = collisionArea.minZ;
    header.maxZ = collisionArea.maxZ;
    header.scaleX = scale.x;
    header.scaleY = scale.y;
    header.scaleZ = scale.z;
    header.terrainSize = terrain->GetTerrainSize();
    header.gridResolution = terrain->GetGridResolution();
    header.edgeFactor = terrain->GetTessellationEdgeFactor();
    header.innerFactor = terrain->GetTessellationInnerFactor();
    header.heightScaler = terrain->GetHeightScaler();

    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    file.write(
        reinterpret_cast<const char*>(vertices.data()),
        sizeof(Vector3) * vertices.size());
    file.write(
        reinterpret_cast<const char*>(indices.data()),
        sizeof(uint32_t) * indices.size());

    if (!file)
    {
        vxMessage = "Terrain .vx save failed: write failed.";
        return false;
    }

    vxMessage = "Terrain .vx saved: " + filepath.generic_string();
    return true;
}

void TerrainMeshCollider::ApplyOwnerScale(std::vector<Vector3>& vertices) const
{
    Actor* actor = GetOwnerAsActor();
    const Vector3 scale = actor->transform.scale;
    for (Vector3& vertex : vertices)
    {
        vertex.x *= scale.x;
        vertex.y *= scale.y;
        vertex.z *= scale.z;
    }
}

void TerrainMeshCollider::UpdateShape(
    const std::vector<Vector3>& vertices,
    const std::vector<uint32_t>& indices)
{
    if (vertices.empty() || indices.empty())
    {
        return;
    }

    PxPhysics* physics = PhysicsManager::Instance().GetPhysics();
    PxCookingParams* cookingParams = PhysicsManager::Instance().GetCooking();
    PxRigidActor* rigidActor = rigidbody->GetRigidActor();

    _ASSERT_EXPR(physics != nullptr, L"PhysX is not initialized.");
    _ASSERT_EXPR(cookingParams != nullptr, L"PhysX cooking is not initialized.");
    _ASSERT_EXPR(rigidActor != nullptr, L"TerrainMeshCollider Rigidbody has no PhysX actor.");
    _ASSERT_EXPR(rigidActor->is<PxRigidStatic>() != nullptr, L"TerrainMeshCollider requires RigidbodyStatic.");

    ReleaseShape();

    std::vector<PxVec3> pxVertices;
    pxVertices.reserve(vertices.size());

    for (const Vector3& vertex : vertices)
    {
        pxVertices.emplace_back(vertex.x, vertex.y, vertex.z);
    }

    PxTriangleMeshDesc meshDesc{};
    meshDesc.points.count = static_cast<PxU32>(pxVertices.size());
    meshDesc.points.stride = sizeof(PxVec3);
    meshDesc.points.data = pxVertices.data();

    meshDesc.triangles.count = static_cast<PxU32>(indices.size() / 3);
    meshDesc.triangles.stride = 3 * sizeof(uint32_t);
    meshDesc.triangles.data = indices.data();

    PxDefaultMemoryOutputStream writeBuffer;
    const bool cooked = PxCookTriangleMesh(*cookingParams, meshDesc, writeBuffer);
    _ASSERT_EXPR(cooked, L"Failed to cook Terrain TriangleMesh.");

    if (!cooked)
    {
        return;
    }

    PxDefaultMemoryInputData readBuffer(writeBuffer.getData(), writeBuffer.getSize());
    PxTriangleMesh* triangleMesh = physics->createTriangleMesh(readBuffer);
    _ASSERT_EXPR(triangleMesh != nullptr, L"Failed to create Terrain TriangleMesh.");

    if (triangleMesh == nullptr)
    {
        return;
    }

    shape = physics->createShape(PxTriangleMeshGeometry(triangleMesh), *material);
    triangleMesh->release();

    _ASSERT_EXPR(shape != nullptr, L"Failed to create TerrainMeshCollider shape.");

    if (shape == nullptr)
    {
        return;
    }

    shape->userData = this;

    Actor* actor = GetOwnerAsActor();
    PhysicsManager::SetLayerToShape(shape, actor->GetLayer());
    rigidActor->attachShape(*shape);
}

void TerrainMeshCollider::Render(const RenderContext& rc)
{
    if (!ShouldRenderColliderDebug(this)) return;
    if (!rc.renderSettings.showDebug) return;

    Terrain* terrain = owner->GetComponent<Terrain>();
    if (terrain != nullptr)
    {
        const float terrainSize = terrain->GetTerrainSize();
        const float halfTerrainSize = terrainSize * 0.5f;

        // collision range
        Game::Graphics::Instance().GetShapeRenderer()->DrawBox(
            Vector3(
            (collisionArea.minX + collisionArea.maxX - 1.0f) * 0.5f * halfTerrainSize,
            0.0f,
            (collisionArea.minZ + collisionArea.maxZ - 1.0f) * 0.5f * halfTerrainSize),
            Vector3::Zero,
            Vector3(
            (collisionArea.maxX - collisionArea.minX) * halfTerrainSize,
            0.1f,
            (collisionArea.maxZ - collisionArea.minZ) * halfTerrainSize),
            Color(0.0f, 1.0f, 0.0f, 1.0f));
    }

}

void TerrainMeshCollider::DrawGUI()
{
	isOpenGUI = ImGui::TreeNode(ICON_FA_SHAPES " TerrainMeshCollider");
    if (isOpenGUI)
    {
        Terrain* terrain = owner->GetComponent<Terrain>();
        if (terrain != nullptr && ImGui::TreeNode("Collision Area AABB"))
        {
            ClampCollisionArea();

            ImGui::DragFloat("Min X", &collisionArea.minX, 0.01f, 0.0f, 1.0f);
            ImGui::DragFloat("Max X", &collisionArea.maxX, 0.01f, 0.0f, 1.0f);
            ImGui::DragFloat("Min Z", &collisionArea.minZ, 0.01f, 0.0f, 1.0f);
            ImGui::DragFloat("Max Z", &collisionArea.maxZ, 0.01f, 0.0f, 1.0f);

            if (ImGui::Button("Reset Full Terrain Area"))
            {
				collisionArea.minX = 0.0f;
				collisionArea.minZ = 0.0f;
				collisionArea.maxX = 1.0f;
				collisionArea.maxZ = 1.0f;
            }

            ImGui::TreePop();
        }

        if (ImGui::Button("Remake Terrain .vx Collider"))
        {
            RequestGpuRebuild();
        }

        ImGui::SameLine();
        if (ImGui::Button("Load Terrain .vx Collider"))
        {
            std::vector<Vector3> vertices;
            std::vector<uint32_t> indices;
            if (LoadCachedMesh(vertices, indices))
            {
                UpdateShape(vertices, indices);
                debugVertices = vertices;
                debugIndices = indices;
            }
        }

        if (ImGui::Button("Save Current Terrain .vx Collider"))
        {
            SaveCachedMesh(debugVertices, debugIndices);
        }

        if (terrain != nullptr)
        {
            ImGui::Text("HeightMap: %d x %d",
                terrain->GetHeightMapWidth(),
                terrain->GetHeightMapHeight());
            ImGui::Text("VX: %s", terrain->GetColliderVertexPath().generic_string().c_str());
        }
        ImGui::Text("%s", vxMessage.c_str());
        ImGui::Text("Vertices: %d", static_cast<int>(debugVertices.size()));
        ImGui::Text("Triangles: %d", static_cast<int>(debugIndices.size() / 3));

        if (ImGui::TreeNode(ICON_FA_GRIP_LINES "Material"))
        {
            float staticFriction = material->getStaticFriction();
            float dynamicFriction = material->getDynamicFriction();
            float restitution = material->getRestitution();

            if (ImGui::DragFloat("Static Friction", &staticFriction, 0.01f, 0.0f, 1.0f))
            {
                material->setStaticFriction(staticFriction);
            }

            if (ImGui::DragFloat("Dynamic Friction", &dynamicFriction, 0.01f, 0.0f, 1.0f))
            {
                material->setDynamicFriction(dynamicFriction);
            }

            if (ImGui::DragFloat("Restitution", &restitution, 0.01f, 0.0f, 1.0f))
            {
                material->setRestitution(restitution);
            }

            ImGui::TreePop();
        }

        ImGui::TreePop();
    }
}
