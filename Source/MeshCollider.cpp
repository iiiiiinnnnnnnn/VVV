// MeshCollider.cpp

#include "MeshCollider.h"
#include "RenderContext.h"
#include "Rigidbody.h"
#include "Graphics.h"
#include "IconsFontAwesome5.h"

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

MeshCollider::MeshCollider(Object* owner, LayerId layerId, Rigidbody* rigidbody, Model* model, PxMaterial* material)
    : PhysicsComponent(owner, layerId), rigidbody(rigidbody), model(model), useConvex(false), quantizedCount(32), material(material)
{
    this->material = material ? material : PhysicsManager::Instance().GetDefaultMaterial();
    UpdateShape();
}

MeshCollider::MeshCollider(Object* owner, LayerId layerId, Rigidbody* rigidbody, Model* model, bool useConvex, unsigned int quantizedCount, PxMaterial* material)
    : PhysicsComponent(owner, layerId), rigidbody(rigidbody), model(model), useConvex(useConvex), quantizedCount(quantizedCount), material(material)
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