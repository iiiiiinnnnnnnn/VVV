// MeshCollider.h

#pragma once

#include "CollidersDef.h"

class Model;

class MeshCollider : public PhysicsComponent
{
public:
    // TriangleMesh（Static用）
    MeshCollider(Object* owner, LayerId layerId, Rigidbody* rigidbody, Model* model, PxMaterial* material = nullptr);
    MeshCollider(Object* owner, LayerId layerId, Rigidbody* rigidbody, Model* model, const Vector3& localScale, PxMaterial* material = nullptr);
    // ConvexMesh（Dynamic用）
    MeshCollider(Object* owner, LayerId layerId, Rigidbody* rigidbody, Model* model, bool useConvex, unsigned int quantizedCount = 32, PxMaterial* material = nullptr);
    MeshCollider(Object* owner, LayerId layerId, Rigidbody* rigidbody, Model* model, const Vector3& localScale, bool useConvex, unsigned int quantizedCount = 32, PxMaterial* material = nullptr);

    void Render(const RenderContext& rc) override;
    void DrawGUI() override;
    void SetLocalScale(const Vector3& scale);
    void SetCollisionEnabled(bool enabled);
	const char* GetDebugName() const override { return ICON_FA_SHAPES " MeshCollider"; }

    bool GetBounds(Vector3& center, Vector3& size) const;
    Vector3 GetWorldPosition() const;
private:
    void UpdateShape();
    void DetachShapes();
    Matrix MakeLocalVertexTransform(const Matrix& nodeTransform) const;
    Rigidbody* rigidbody = nullptr;
    PxMaterial* material = nullptr;
    Model* model = nullptr;
    Vector3 localScale = Vector3::One;
    bool useConvex = false;
    bool collisionEnabled = true;
    unsigned int quantizedCount = 32;
};



