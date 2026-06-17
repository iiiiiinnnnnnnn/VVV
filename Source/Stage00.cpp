// Stage00.cpp

#include "Stage00.h"
#include "ResourceManager.h"

Stage00::Stage00() : Actor("Stage00", "Stage", Layer::Stage)
{
    // 参考用に残しているがStage00.glbを消したのでこのステージは無意味
    #if 0
    std::shared_ptr<Model> model =
        ResourceManager::Instance().LoadModel("Data/Model/Stage00.glb");

    transform.SetScale(100.0f, 100.0f, 100.0f);
    transform.SetAngle(0, RAD(-90.0f), 0);
    model->UpdateTransform(transform.matrix);

    Matrix identity = Matrix::Identity;
    auto* rb = AddComponent<RigidbodyStatic>(identity);

    AddComponent<MeshCollider>(rb, model.get());

    // モデルレンダラー生成
    shaderParamWithMaterialName =
    {
        {
            "planem",
        {
            {"metalness", 0.5f},
        {"roughness", 0.0f},
        {"occlusion", 0.0f},
        {"occlusionStrength", 0.8f}
    }
        },
        {
            "standardSurface1",
        {
            {"metalness", 1.0f},
        {"roughness", 0.1f},
        {"occlusion", 0.5f},
        {"occlusionStrength", 1.0f}
    }
        }
    };
    AddComponent<ModelRenderComponent>(
        model, ModelShaderId::PBR, shaderParamWithMaterialName);
    #endif
}

void Stage00::OnUpdate()
{

}

void Stage00::OnDrawGUI()
{

}
