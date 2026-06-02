// Stage00.cpp

#include "Stage00.h"
#include "ResourceManager.h"

Stage00::Stage00() : Actor("Stage00", "Stage00", "Default")
{
    std::shared_ptr<Model> model =
        ResourceManager::Instance().LoadModel("Data/Model/Stage00.glb");

    transform.SetScale(100.0f, 100.0f, 100.0f);
    transform.SetAngle(0, RAD(-90.0f), 0);
    model->UpdateTransform(transform.matrix); // ノードのworldTransformをスケール・回転込みで確定

    // RigidbodyStaticは原点・単位回転で作る
    // （MeshColliderの頂点がすでにworldTransform適用済みのワールド座標になっているため、
    //   PhysXのActorにも回転・スケールを乗せると二重適用になる）
    Matrix identity = Matrix::Identity;
    auto* rb = AddComponent<RigidbodyStatic>(identity);

    // コライダー生成（mesh.node->worldTransformのワールド座標で頂点を登録）
    AddComponent<MeshCollider>(rb, model.get());

    // モデルレンダラー生成
    shaderParamWithMaterialName =
    {
        {
            "planem",
        {
            {"metalness", 0.0f},
        {"roughness", 1.0f},
        {"occlusionStrength", 1.0f}
    }
        },
        {
            "standardSurface1",
        {
            {"metalness", 1.0f},
        {"roughness", 0.1f},
        {"occlusionStrength", 1.0f}
    }
        }
    };
    AddComponent<ModelRenderComponent>(
        model, ModelShaderId::PBR, shaderParamWithMaterialName);
}

void Stage00::OnUpdate()
{
}

void Stage00::OnDrawGUI()
{
}
