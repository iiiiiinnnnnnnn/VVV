// MeshCollider.cpp

#include "MeshCollider.h"
#include "RenderContext.h"
#include "Rigidbody.h"
#include "Graphics.h"
#include "IconsFontAwesome5.h"

Matrix MeshCollider::MakeLocalVertexTransform(const Matrix& nodeTransform) const
{
    Transform* transform = owner->GetComponent<Transform>();
    Vector3 ownerScale = transform ? transform->scale : Vector3::One;
    return nodeTransform * Matrix::CreateScale(ownerScale * localScale);
}
bool MeshCollider::GetBounds(Vector3& center, Vector3& size) const
{
    if (!rigidbody) return false;
    if (!rigidbody->GetRigidActor()) return false;
    if (!model) return false;

    Vector3 minPosition(FLT_MAX, FLT_MAX, FLT_MAX);
    Vector3 maxPosition(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    bool hasVertex = false;
    Matrix actorTransform = Conv::ToMatrix(rigidbody->GetRigidActor()->getGlobalPose());

    for (const Model::Mesh& mesh : model->GetMeshes())
    {
        if (!mesh.isDraw) continue;
        if (!mesh.node) continue;
        Matrix localVertexTransform = MakeLocalVertexTransform(mesh.node->globalTransform);
        for (const Model::Vertex& vertex : mesh.vertices)
        {
            Vector3 position = Vector3::Transform(vertex.position, localVertexTransform);
            position = Vector3::Transform(position, actorTransform);

            if (position.x < minPosition.x) minPosition.x = position.x;
            if (position.y < minPosition.y) minPosition.y = position.y;
            if (position.z < minPosition.z) minPosition.z = position.z;

            if (position.x > maxPosition.x) maxPosition.x = position.x;
            if (position.y > maxPosition.y) maxPosition.y = position.y;
            if (position.z > maxPosition.z) maxPosition.z = position.z;

            hasVertex = true;
        }
    }

    if (!hasVertex) return false;

    center = (minPosition + maxPosition) * 0.5f;
    size = maxPosition - minPosition;
    return true;
}
void MeshCollider::Render(const RenderContext& rc)
{
	if (!showDebug) return;

    Vector3 minPosition(FLT_MAX, FLT_MAX, FLT_MAX);
    Vector3 maxPosition(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    bool hasVertex = false;

    for (const Model::Mesh& mesh : model->GetMeshes())
    {
        if (!mesh.isDraw) continue;
        if (!mesh.node) continue;
        Matrix localVertexTransform = MakeLocalVertexTransform(mesh.node->globalTransform);
        Matrix actorTransform = Conv::ToMatrix(rigidbody->GetRigidActor()->getGlobalPose());
        for (const Model::Vertex& vertex : mesh.vertices)
        {
            Vector3 position = Vector3::Transform(vertex.position, localVertexTransform);
            position = Vector3::Transform(position, actorTransform);

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

MeshCollider::MeshCollider(Object* owner, LayerId layerId, Rigidbody* rigidbody, Model* model, PxMaterial* material)
    : PhysicsComponent(owner, layerId), rigidbody(rigidbody), model(model), useConvex(false), quantizedCount(32), material(material)
{
    this->material = material ? material : PhysicsManager::Instance().GetDefaultMaterial();
}

MeshCollider::MeshCollider(Object* owner, LayerId layerId, Rigidbody* rigidbody, Model* model, const Vector3& localScale, PxMaterial* material)
    : PhysicsComponent(owner, layerId), rigidbody(rigidbody), model(model), localScale(localScale), useConvex(false), quantizedCount(32), material(material)
{
    this->material = material ? material : PhysicsManager::Instance().GetDefaultMaterial();
}
MeshCollider::MeshCollider(Object* owner, LayerId layerId, Rigidbody* rigidbody, Model* model, bool useConvex, unsigned int quantizedCount, PxMaterial* material)
    : PhysicsComponent(owner, layerId), rigidbody(rigidbody), model(model), useConvex(useConvex), quantizedCount(quantizedCount), material(material)
{
    this->material = material ? material : PhysicsManager::Instance().GetDefaultMaterial();
}

MeshCollider::MeshCollider(Object* owner, LayerId layerId, Rigidbody* rigidbody, Model* model, const Vector3& localScale, bool useConvex, unsigned int quantizedCount, PxMaterial* material)
    : PhysicsComponent(owner, layerId), rigidbody(rigidbody), model(model), localScale(localScale), useConvex(useConvex), quantizedCount(quantizedCount), material(material)
{
    this->material = material ? material : PhysicsManager::Instance().GetDefaultMaterial();
}
void MeshCollider::OnAwake()
{
    UpdateShape();
}

void MeshCollider::SetLocalScale(const Vector3& scale)
{
    if ((localScale - scale).LengthSquared() < 0.000001f) return;
    localScale = scale;
    if (collisionEnabled) UpdateShape();
}

void MeshCollider::SetCollisionEnabled(bool enabled)
{
    if (collisionEnabled == enabled) return;

    collisionEnabled = enabled;
    SetActive(enabled);

    if (collisionEnabled)
        UpdateShape();
    else
        DetachShapes();
}

void MeshCollider::DetachShapes()
{
    if (!rigidbody) return;

    PxRigidActor* rigidActor = rigidbody->GetRigidActor();
    if (!rigidActor) return;

    PxU32 shapeCount = rigidActor->getNbShapes();
    std::vector<PxShape*> shapes(shapeCount);
    rigidActor->getShapes(shapes.data(), shapeCount);
    for (PxShape* shape : shapes)
    {
        rigidActor->detachShape(*shape);
    }
}

void MeshCollider::UpdateShape()
{
    if (!collisionEnabled || !rigidbody || !model) return;

    PxRigidActor* rigidActor = rigidbody->GetRigidActor();
    if (!rigidActor) return;

    PxPhysics* physics = PhysicsManager::Instance().GetPhysics();
    PxCookingParams* cookingParams = PhysicsManager::Instance().GetCooking();

    DetachShapes();

    for (const Model::Mesh& mesh : model->GetMeshes())
    {
        if (!mesh.isDraw) continue;
        std::vector<PxVec3> vertices;
        Matrix nodeTransform = mesh.node ? mesh.node->globalTransform : Matrix::Identity;
        Matrix localVertexTransform = MakeLocalVertexTransform(nodeTransform);
        for (const Model::Vertex& v : mesh.vertices)
        {
            Vector3 pos = Vector3::Transform(v.position, localVertexTransform);
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
        PhysicsManager::SetLayerToShape(shape, layerId);
        rigidActor->attachShape(*shape);

        shape->release();
    }
}

void MeshCollider::DrawGUI()
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
}






