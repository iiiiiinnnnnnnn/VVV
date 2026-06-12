// Collider.cpp

#include "Collider.h"
#include "Actor.h"
#include "Graphics.h"
#include "ShapeRenderer.h"

BoxCollider::BoxCollider(Object* owner, Rigidbody* rigidbody, const Vector3& size, PxMaterial* material)
    : Component(owner), material(material), size(size), rigidbody(rigidbody)
{
    // エラー用
    Component::GetOwnerAsActor();

    this->material = material ? material : PhysicsManager::Instance().GetDefaultMaterial();
    UpdateShape();
}

void BoxCollider::Render(const RenderContext& rc)
{
    if (!rc.renderSettings.showDebug) return;

    Game::Graphics::Instance().GetShapeRenderer()->DrawBox(
        rigidbody->GetPosition(), Vector3::Zero, size, {0.0f, 1.0f, 0.0f, 1.0f});
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
        PxBoxGeometry(size.x * 0.5f, size.y * 0.5f, size.z * 0.5f), *material);
    shape->setLocalPose(PxTransform(PxQuat(DirectX::XM_PIDIV2, PxVec3(0, 0, 1))));
	rigidActor->attachShape(*shape);

    // ownerのlayerをシェイプに反映
    Actor* actor = Component::GetOwnerAsActor();
    PhysicsManager::SetLayerToShape(shape, actor->GetLayer());
}

void BoxCollider::DrawGUI()
{
    if (ImGui::TreeNode("BoxCollider"))
    {
        bool changed = false;
        changed |= ImGui::DragFloat3("Size", &size.x, 0.01f, 0.01f, 100.0f);

        if (changed) UpdateShape();

        if (ImGui::TreeNode("Material")) {
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
    : Component(owner), rigidbody(rigidbody), material(material), radius(radius), height(height)
{
	// エラー用
    Component::GetOwnerAsActor();

    this->material = material ? material : PhysicsManager::Instance().GetDefaultMaterial();
    UpdateShape();
}

void CapsuleCollider::Render(const RenderContext& rc)
{
    if (!rc.renderSettings.showDebug) return;

    Game::Graphics::Instance().GetShapeRenderer()->DrawCapsule(
        PX_TRANSFORM_TO_MATRIX(rigidbody->GetRigidActor()->getGlobalPose()), radius, height, {0.0f, 1.0f, 0.0f, 1.0f});
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
    PxTransform localPose(
        PxVec3(0, height * 0.5f + radius, 0),
        PxQuat(DirectX::XM_PIDIV2, PxVec3(0, 0, 1))
    );
    shape->setLocalPose(localPose);
    rigidActor->attachShape(*shape);

    // ownerのlayerをシェイプに反映
    Actor* actor = Component::GetOwnerAsActor();
    PhysicsManager::SetLayerToShape(shape, actor->GetLayer());
}

void CapsuleCollider::DrawGUI()
{
    if (ImGui::TreeNode("CapsuleCollider"))
    {
        bool changed = false;
        changed |= ImGui::DragFloat("Radius", &radius, 0.01f, 0.01f, 100.0f);
        changed |= ImGui::DragFloat("Height", &height, 0.01f, 0.01f, 100.0f);
		if (radius < 0.01f) radius = 0.01f;

        if (changed) UpdateShape();

        if (ImGui::TreeNode("Material")) {
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
    : Component(owner), rigidbody(rigidbody), material(material), radius(radius)
{
	// エラー用
    Component::GetOwnerAsActor();

    this->material = material ? material : PhysicsManager::Instance().GetDefaultMaterial();
    UpdateShape();
}

void SphereCollider::Render(const RenderContext& rc)
{
	if (!rc.renderSettings.showDebug) return;

    Game::Graphics::Instance().GetShapeRenderer()->DrawSphere(
        rigidbody->GetPosition() + Vector3(0, radius, 0), radius, { 0.0f, 1.0f, 0.0f, 1.0f });
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
    shape->setLocalPose(
        PxTransform(PxVec3(0, radius, 0))
    );
    rigidActor->attachShape(*shape);

    // ownerのlayerをシェイプに反映
    Actor* actor = Component::GetOwnerAsActor();
    PhysicsManager::SetLayerToShape(shape, actor->GetLayer());
}

void SphereCollider::DrawGUI()
{
    if (ImGui::TreeNode("SphereCollider"))
    {
        bool changed = false;
        changed |= ImGui::DragFloat("Radius", &radius, 0.01f, 0.01f, 100.0f);
        if (radius < 0.01f) radius = 0.01f;

        if (changed) UpdateShape();

        if (ImGui::TreeNode("Material")) {
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
    if (!rc.renderSettings.showDebug) return;

    PrimitiveRenderer* pr = Game::Graphics::Instance().GetPrimitiveRenderer();
    Color color(0.0f, 1.0f, 1.0f, 1.0f);

    for (const Model::Mesh& mesh : model->GetMeshes())
    {
        if (!mesh.isDraw) continue;

        const auto& verts   = mesh.vertices;
        const auto& indices = mesh.indices;

        for (size_t i = 0; i < indices.size(); i += 3)
        {
            Vector3 v0 = Vector3::Transform(verts[indices[i + 0]].position, mesh.node->worldTransform);
            Vector3 v1 = Vector3::Transform(verts[indices[i + 1]].position, mesh.node->worldTransform);
            Vector3 v2 = Vector3::Transform(verts[indices[i + 2]].position, mesh.node->worldTransform);

            pr->DrawLine(v0, v1, color, color);
            pr->DrawLine(v1, v2, color, color);
            pr->DrawLine(v2, v0, color, color);
        }
    }

    pr->Render(
        rc.deviceContext,
        rc.camera->GetView(),
        rc.camera->GetProjection(),
        D3D11_PRIMITIVE_TOPOLOGY_LINELIST
    );
}

MeshCollider::MeshCollider(Object* owner, Rigidbody* rigidbody, Model* model, PxMaterial* material)
    : Component(owner), rigidbody(rigidbody), model(model), useConvex(false), quantizedCount(32), material(material)
{
    this->material = material ? material : PhysicsManager::Instance().GetDefaultMaterial();
    UpdateShape();
}

MeshCollider::MeshCollider(Object* owner, Rigidbody* rigidbody, Model* model, bool useConvex, unsigned int quantizedCount, PxMaterial* material)
    : Component(owner), rigidbody(rigidbody), model(model), useConvex(useConvex), quantizedCount(quantizedCount), material(material)
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
            PxConvexMeshDesc convexDesc;
            convexDesc.points.count  = (PxU32)vertices.size();
            convexDesc.points.stride = sizeof(PxVec3);
            convexDesc.points.data   = vertices.data();
            convexDesc.flags         = PxConvexFlag::eCOMPUTE_CONVEX | PxConvexFlag::eQUANTIZE_INPUT;
            convexDesc.quantizedCount = quantizedCount;

            PxDefaultMemoryOutputStream writeBuffer;
            PxCookConvexMesh(*cookingParams, convexDesc, writeBuffer);
            PxDefaultMemoryInputData readBuffer(writeBuffer.getData(), writeBuffer.getSize());
            PxConvexMesh* convexMesh = physics->createConvexMesh(readBuffer);

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
            PxCookTriangleMesh(*cookingParams, meshDesc, writeBuffer);
            PxDefaultMemoryInputData readBuffer(writeBuffer.getData(), writeBuffer.getSize());
            PxTriangleMesh* triangleMesh = physics->createTriangleMesh(readBuffer);

            shape = physics->createShape(PxTriangleMeshGeometry(triangleMesh), *material);
            triangleMesh->release();
        }

        rigidActor->attachShape(*shape);

        // ownerのlayerをシェイプに反映
        Actor* actor = Component::GetOwnerAsActor();
        PhysicsManager::SetLayerToShape(shape, actor->GetLayer());

        shape->release();
    }
}

void MeshCollider::DrawGUI()
{
    if (ImGui::TreeNode("MeshCollider"))
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

BoneSphereCollider::BoneSphereCollider(Object* owner, Model* model, int nodeIndex, float radius, Matrix offset, PxMaterial* material)
    : Component(owner), model(model), nodeIndex(nodeIndex), radius(radius), offset(offset), material(material)
{
    Actor* actor = Component::GetOwnerAsActor();

    this->material = material ? material : PhysicsManager::Instance().GetDefaultMaterial();

    PxPhysics* physics = PhysicsManager::Instance().GetPhysics();

    // ボーンの初期位置でKinematic Dynamicを生成
    Matrix boneMat = model->GetNodes()[nodeIndex].worldTransform;
    Vector3 scale, pos;
    Quaternion rot;
    boneMat.Decompose(scale, rot, pos);

    PxTransform t(
        PxVec3(pos.x, pos.y, pos.z),
        PxQuat(rot.x, rot.y, rot.z, rot.w)
    );
    ghostActor = physics->createRigidDynamic(t);
    ghostActor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);

	ghostActor->userData = owner;

    // Triggerシェイプ（物理応答なし → プレイヤーが飛ばない）
    shape = physics->createShape(PxSphereGeometry(radius), *this->material, true);
    shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
    shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, true);

    // ownerのlayerをシェイプに反映
    PhysicsManager::SetLayerToShape(shape, actor->GetLayer());

    ghostActor->attachShape(*shape);
    shape->release();

    PhysicsManager::Instance().GetSceneContext().GetScene()->addActor(*ghostActor);
}

BoneSphereCollider::~BoneSphereCollider()
{
    if (ghostActor)
    {
        PhysicsManager::Instance().GetSceneContext().GetScene()->removeActor(*ghostActor);
        ghostActor->release();
        ghostActor = nullptr;
    }
}

void BoneSphereCollider::LateUpdate()
{
    UpdateShape();
}

void BoneSphereCollider::UpdateShape()
{
    if (!ghostActor || nodeIndex < 0) return;

    Matrix boneMat = model->GetNodes()[nodeIndex].worldTransform;
    Vector3 scale, pos;
    Quaternion rot;
    boneMat.Decompose(scale, rot, pos);

    Matrix base = Matrix::CreateFromQuaternion(rot) * Matrix::CreateTranslation(pos);
    Matrix world = offset * base;
    ghostActor->setKinematicTarget(MATRIX_TO_PX_TRANSFORM(world));
}

void BoneSphereCollider::Render(const RenderContext& rc)
{
    if (!rc.renderSettings.showDebug || !ghostActor) return;

    PxTransform t = ghostActor->getGlobalPose();
    Matrix m = PX_TRANSFORM_TO_MATRIX(t);
    Vector3 pos(m._41, m._42, m._43);

    Game::Graphics::Instance().GetShapeRenderer()->DrawSphere(
        pos, radius, Color(1.0f, 0.0f, 1.0f, 1.0f));
}

void BoneSphereCollider::DrawGUI()
{
    if (ImGui::TreeNode("BoneSphereCollider"))
    {
        ImGui::Text("NodeIndex: %d", nodeIndex);
        ImGui::DragFloat("Radius", &radius, 0.01f, 0.01f, 10.0f);
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
        if (ImGui::DragFloat3("OffsetRotation", &euler.x, 0.01f))
        {
            offset = Matrix::CreateScale(scale) *
                Matrix::CreateFromYawPitchRoll(RAD(euler.y), RAD(euler.x), RAD(euler.z)) *
                Matrix::CreateTranslation(pos);
        }
        ImGui::TreePop();
    }
}
