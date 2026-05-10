// Collider.cpp

#include "Collider.h"

BoxCollider::BoxCollider(Actor* owner, Rigidbody* rigidbody, const Vector3& size, PxMaterial* material)
    : Component(owner), material(material), size(size), rigidbody(rigidbody)
{
    this->material = material ? material : PhysicsManager::Instance().GetDefaultMaterial();
    UpdateShape();
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

void BoxCollider::DrawGUI(float elapsedTime)
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

CapsuleCollider::CapsuleCollider(Actor* owner, Rigidbody* rigidbody, float radius, float height, PxMaterial* material)
    : Component(owner), rigidbody(rigidbody), material(material), radius(radius), height(height)
{
    this->material = material ? material : PhysicsManager::Instance().GetDefaultMaterial();
    UpdateShape();
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

void CapsuleCollider::DrawGUI(float elapsedTime)
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

SphereCollider::SphereCollider(Actor* owner, Rigidbody* rigidbody, float radius, PxMaterial* material)
    : Component(owner), rigidbody(rigidbody), material(material), radius(radius)
{
    this->material = material ? material : PhysicsManager::Instance().GetDefaultMaterial();
    UpdateShape();
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
    shape->setLocalPose(PxTransform(PxQuat(DirectX::XM_PIDIV2, PxVec3(0, 0, 1))));
    rigidActor->attachShape(*shape);
}

void SphereCollider::DrawGUI(float elapsedTime)
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

MeshCollider::MeshCollider(Actor* owner, Rigidbody* rigidbody, Model* model, PxMaterial* material)
	: Component(owner), rigidbody(rigidbody), model(model), material(material)
{
    this->material = material ? material : PhysicsManager::Instance().GetDefaultMaterial();
    UpdateShape();
}

void MeshCollider::UpdateShape()
{
    PxPhysics* physics = PhysicsManager::Instance().GetPhysics();
    PxCookingParams* cookingParams = PhysicsManager::Instance().GetCooking();
    PxRigidActor* rigidActor = rigidbody->GetRigidActor();

    for (const Model::Mesh& mesh : model->GetMeshes())
    {
        if (!mesh.isDraw) continue;

        std::vector<PxVec3> vertices;
        for (const Model::Vertex& v : mesh.vertices)
        {
            // ノードのワールド行列を適用
            Vector3 pos = Vector3::Transform(v.position, mesh.node->worldTransform);
            vertices.push_back(PxVec3(pos.x, pos.y, pos.z));
        }

        PxTriangleMeshDesc meshDesc;
        meshDesc.points.count = (PxU32)vertices.size();
        meshDesc.points.stride = sizeof(PxVec3);
        meshDesc.points.data = vertices.data();
        meshDesc.triangles.count = (PxU32)(mesh.indices.size() / 3);
        meshDesc.triangles.stride = 3 * sizeof(PxU32);
        meshDesc.triangles.data = mesh.indices.data();

        PxDefaultMemoryOutputStream writeBuffer;
        PxCookTriangleMesh(*cookingParams, meshDesc, writeBuffer);

        PxDefaultMemoryInputData readBuffer(writeBuffer.getData(), writeBuffer.getSize());
        PxTriangleMesh* triangleMesh = physics->createTriangleMesh(readBuffer);

        PxShape* shape = physics->createShape(
            PxTriangleMeshGeometry(triangleMesh), *material);

        rigidActor->attachShape(*shape);
        shape->release();
        triangleMesh->release();
    }
}

void MeshCollider::DrawGUI(float elapsedTime)
{
    if (ImGui::TreeNode("MeshCollider"))
    {
        ImGui::Text("TriangleMesh");
        ImGui::TreePop();
    }
}
