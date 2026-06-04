// Collider.cpp

#include "Collider.h"
#include "Actor.h"
#include "Graphics.h"
#include "ShapeRenderer.h"

BoxCollider::BoxCollider(Object* owner, Rigidbody* rigidbody, const Vector3& size, PxMaterial* material)
    : Component(owner), material(material), size(size), rigidbody(rigidbody)
{
    Actor* actor = dynamic_cast<Actor*>(owner);
    _ASSERT_EXPR(actor != nullptr, L"Object is not Actor");

    this->material = material ? material : PhysicsManager::Instance().GetDefaultMaterial();
    UpdateShape();

    // ownerのlayerをシェイプに反映
    PhysicsManager::SetLayerToShape(shape, actor->GetLayer());
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
    Actor* actor = dynamic_cast<Actor*>(owner);
    _ASSERT_EXPR(actor != nullptr, L"Object is not Actor");

    this->material = material ? material : PhysicsManager::Instance().GetDefaultMaterial();
    UpdateShape();

    // ownerのlayerをシェイプに反映
    PhysicsManager::SetLayerToShape(shape, actor->GetLayer());
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
    Actor* actor = dynamic_cast<Actor*>(owner);
    _ASSERT_EXPR(actor != nullptr, L"Object is not Actor");

    this->material = material ? material : PhysicsManager::Instance().GetDefaultMaterial();
    UpdateShape();

    // ownerのlayerをシェイプに反映
    PhysicsManager::SetLayerToShape(shape, actor->GetLayer());
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

        Actor* actor = dynamic_cast<Actor*>(owner);
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

BoneSphereCollider::BoneSphereCollider(Object* owner, Model* model, int nodeIndex, float radius, PxMaterial* material)
	: Component(owner), model(model), nodeIndex(nodeIndex), radius(radius), material(material)
{
}

void BoneSphereCollider::Render(const RenderContext& rc)
{
}

void BoneSphereCollider::DrawGUI()
{
}

void BoneSphereCollider::UpdateShape()
{
}
