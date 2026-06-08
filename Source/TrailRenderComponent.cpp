// TrailRenderComponent.cpp

#include "TrailRenderComponent.h"
#include "Graphics.h"
#include "GameTime.h"

TrailRenderComponent::TrailRenderComponent(
    Object* owner,
    Model* model,
    int   nodeIndex,
    float rootOffset,
    float tipOffset,
    float tipRatio,
    float lifeTime,
    int   maxPoints)
    : Component(owner)
    , model(model)
    , nodeIndex(nodeIndex)
    , rootOffset(rootOffset)
    , tipOffset(tipOffset)
	, tipRatio(tipRatio)
    , lifeTime(lifeTime)
    , maxPoints(maxPoints)
{
}

void TrailRenderComponent::LateUpdate()
{
    float dt = Game::Time::deltaTime;

    // --- 既存の点を老化させ、寿命切れを削除 ---
    for (auto& p : points)
    {
        p.age += dt;
        float t = p.age / lifeTime;
        float easedT = t * t;  // EaseIn
        p.tip = Vector3::Lerp(p.tipFull, p.root, easedT * tipRatio);
    }

    while (!points.empty() && points.back().age >= lifeTime)
        points.pop_back();

    // --- アクティブでなければサンプリングしない（点は老化・削除だけ進む）---
    if (!IsActive() || stopping) return;

    // --- サンプリング間隔チェック ---
    sampleTimer += dt;
    if (sampleTimer < SAMPLE_INTERVAL) return;
    sampleTimer = 0.0f;

    // --- ノードのワールド行列から root / tip を取得 ---
    const Model::Node& node = model->GetNodes()[nodeIndex];
    const Matrix& wt = node.worldTransform;

    Matrix rootMat = Matrix::CreateTranslation(rootOffset, 0.0f, 0.0f) * wt;
    Matrix tipMat  = Matrix::CreateTranslation(tipOffset,  0.0f, 0.0f) * wt;

    TrailPoint pt;
    pt.root = { rootMat._41, rootMat._42, rootMat._43 };
    pt.tipFull = { tipMat._41,  tipMat._42,  tipMat._43  };
    pt.tip     = pt.root;
    pt.age  = 0.0f;

    points.push_front(pt);

    // 上限を超えたら古いものを削除
    while ((int)points.size() > maxPoints)
        points.pop_back();
}

void TrailRenderComponent::Render(const RenderContext& rc)
{
    const int splineSegment = 50;

    auto& graphics = Game::Graphics::Instance();
    auto dc = rc.deviceContext;
    auto renderState = rc.renderState;

    if (points.size() >= 2)
    {
        // ブレンドステート等の設定
        dc->OMSetBlendState(renderState->GetBlendState(BlendState::Additive), nullptr, 0xFFFFFFFF);
        dc->OMSetDepthStencilState(renderState->GetDepthStencilState(DepthState::TestOnly), 0);
        dc->RSSetState(renderState->GetRasterizerState(RasterizerState::SolidCullNone));

        TrailRenderer* trailRenderer = graphics.GetTrailRenderer();
        int count = (int)points.size();
        for (int i = 0; i < count - 1; ++i)
        {
            for (int j = 0; j < splineSegment; ++j)
            {
                float t = static_cast<float>(j) / splineSegment;

                // 仮想点を作る
                Vector3 p0root = (i == 0)           ? points[0].root * 2 - points[1].root           : points[i - 1].root;
                Vector3 p3root = (i >= count - 2)   ? points[count-1].root * 2 - points[count-2].root : points[i + 2].root;
                Vector3 p0tip  = (i == 0)           ? points[0].tip * 2 - points[1].tip              : points[i - 1].tip;
                Vector3 p3tip  = (i >= count - 2)   ? points[count-1].tip * 2 - points[count-2].tip   : points[i + 2].tip;

                Vector3 interpolatedRoot = Vector3::CatmullRom(p0root, points[i].root, points[i + 1].root, p3root, t);
                Vector3 interpolatedTip  = Vector3::CatmullRom(p0tip,  points[i].tip,  points[i + 1].tip,  p3tip,  t);

                float age0 = points[i].age;
                float age1 = points[i + 1].age;
                float uvY = std::lerp(age0, age1, t) / lifeTime;

                trailRenderer->AddPoint(interpolatedRoot, interpolatedTip, uvY);
            }
        }
        trailRenderer->Render(dc, rc.camera->GetView(), rc.camera->GetProjection());
    }
}

void TrailRenderComponent::DrawGUI()
{
    if (ImGui::TreeNode("TrailRenderComponent"))
    {
        ImGui::Text("Node Index : %d", nodeIndex);
        ImGui::Text("Life Time  : %.2f", lifeTime);
        ImGui::Text("Max Points : %d", maxPoints);
        ImGui::Text("Points     : %d", (int)points.size());

        ImGui::TreePop();
    }
}
