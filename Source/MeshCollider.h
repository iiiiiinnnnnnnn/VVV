// MeshCollider.h

#pragma once

#include "CollidersDef.h"

class Model;

class MeshCollider : public PhysicsComponent
{
public:
    // TriangleMesh（Static用）
    MeshCollider(Object* owner, LayerId layerId, Rigidbody* rigidbody, Model* model, PxMaterial* material = nullptr);
    // ConvexMesh（Dynamic用）
    MeshCollider(Object* owner, LayerId layerId, Rigidbody* rigidbody, Model* model, bool useConvex, unsigned int quantizedCount = 32, PxMaterial* material = nullptr);

    void Render(const RenderContext& rc) override;
    void DrawGUI() override;
	const char* GetDebugName() const override { return ICON_FA_SHAPES " MeshCollider"; }

    Vector3 GetWorldPosition() const;
private:
    void UpdateShape();
    Matrix MakeLocalVertexTransform(const Matrix& nodeTransform) const;
    Rigidbody* rigidbody = nullptr;
    PxMaterial* material = nullptr;
    Model* model = nullptr;
    bool useConvex = false;
    unsigned int quantizedCount = 32;
};
