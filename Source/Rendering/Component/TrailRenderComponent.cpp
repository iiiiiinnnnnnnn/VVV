// TrailRenderComponent.cpp

#include "Rendering/Component/TrailRenderComponent.h"
#include "Rendering/Core/Graphics.h"
#include "Application/Time/GameTime.h"
#include "IconsFontAwesome5.h"

TrailRenderComponent::TrailRenderComponent(
    Object* owner,
    VMDLModel* model,
    int   nodeIndex,
    Color color,
    float rootOffset,
    float tipOffset,
    float tipRatio,
    float lifeTime,
    int   maxPoints,
    Vector3 offsetAngle)
    : Component(owner)
    , model(model)
    , nodeIndex(nodeIndex)
    , color(color)
	, rootOffset(rootOffset, 0.0f, 0.0f)
	, tipOffset(tipOffset, 0.0f, 0.0f)
    , tipRatio(tipRatio)
    , lifeTime(lifeTime)
    , maxPoints(maxPoints)
	, offsetAngle(offsetAngle)
{
}

TrailRenderComponent::TrailRenderComponent(
	Object* owner,
	VMDLModel* model,
	int nodeIndex,
	const Vector3& rootOffset,
	const Vector3& tipOffset,
	Color color,
	float tipRatio,
	float lifeTime,
	int maxPoints,
    Vector3 offsetAngle)
	: Component(owner),
	model(model),
	nodeIndex(nodeIndex),
	rootOffset(rootOffset),
	tipOffset(tipOffset),
	tipRatio(tipRatio),
	lifeTime(lifeTime),
	maxPoints(maxPoints),
	color(color),
	offsetAngle(offsetAngle)
{
}

void TrailRenderComponent::LateUpdate()
{
    float dt = Game::Time::deltaTime;

    for (auto& p : points)
    {
		// 古い点ほど刃先を根元へ縮め、残像の終端が自然に細くなるよう二乗イージングする。
        p.age += dt;
        float t = p.age / lifeTime;
        float easedT = t * t;  // EaseIn
        p.tip = Vector3::Lerp(p.tipFull, p.root, easedT * tipRatio);
    }

    while (!points.empty() && points.back().age >= lifeTime)
        points.pop_back();

    if (!IsActive() || stopping) return;

    sampleTimer += dt;
    if (sampleTimer < SAMPLE_INTERVAL) return;
    sampleTimer = 0.0f;

    const VMDLModel::Node& node = model->GetNodes()[nodeIndex];
    Matrix wt = node.worldTransform;

	wt *= Matrix::CreateFromYawPitchRoll(
        RAD(offsetAngle.y),
        RAD(offsetAngle.x),
        RAD(offsetAngle.z));

    Matrix rootMat = Matrix::CreateTranslation(rootOffset) * wt;
    Matrix tipMat  = Matrix::CreateTranslation(tipOffset) * wt;

    TrailPoint pt;
    pt.root    = { rootMat._41, rootMat._42, rootMat._43 };
    pt.tipFull = { tipMat._41,  tipMat._42,  tipMat._43  };
    pt.tip     = pt.tipFull;
    pt.age     = 0.0f;

    points.push_front(pt);

    while ((int)points.size() > maxPoints)
        points.pop_back();
}

void TrailRenderComponent::BuildTrailVertices()
{
    if (points.size() < 2) return;

    const int splineSegment = 50;
    TrailRenderer* trailRenderer = Game::Graphics::Instance().GetTrailRenderer();
    int count = (int)points.size();

    for (int i = 0; i < count - 1; ++i)
    {
		// 各サンプル間をCatmull-Rom曲線で補間する。端では隣接点を外挿して仮想制御点を作り、
		// 始端と終端でも接線が急に折れないようにする。
        for (int j = 0; j < splineSegment; ++j)
        {
            float t = static_cast<float>(j) / splineSegment;

            Vector3 p0root = (i == 0)         ? points[0].root * 2 - points[1].root               : points[i - 1].root;
            Vector3 p3root = (i >= count - 2) ? points[count-1].root * 2 - points[count-2].root   : points[i + 2].root;
            Vector3 p0tip  = (i == 0)         ? points[0].tip  * 2 - points[1].tip                : points[i - 1].tip;
            Vector3 p3tip  = (i >= count - 2) ? points[count-1].tip  * 2 - points[count-2].tip    : points[i + 2].tip;

            Vector3 interpolatedRoot = Vector3::CatmullRom(p0root, points[i].root, points[i + 1].root, p3root, t);
            Vector3 interpolatedTip  = Vector3::CatmullRom(p0tip,  points[i].tip,  points[i + 1].tip,  p3tip,  t);

            float age0 = points[i].age;
            float age1 = points[i + 1].age;
            float uvY  = std::lerp(age0, age1, t) / lifeTime;

            trailRenderer->AddPoint(interpolatedRoot, interpolatedTip, uvY);
        }
    }
}

void TrailRenderComponent::Render(const RenderContext& rc)
{
}

void TrailRenderComponent::RenderTrail(const RenderContext& rc)
{
    if (points.size() < 2) return;

    BuildTrailVertices();

    auto& graphics   = Game::Graphics::Instance();
    auto  dc          = rc.deviceContext;
    auto  renderState = rc.renderState;

	// 軌跡は加算・深度書き込みなし・両面描画で描き、終了後は標準状態へ必ず戻す。
    dc->OMSetBlendState(renderState->GetBlendState(BlendState::Additive), nullptr, 0xFFFFFFFF);
    dc->OMSetDepthStencilState(renderState->GetDepthStencilState(DepthState::TestOnly), 0);
    dc->RSSetState(renderState->GetRasterizerState(RasterizerState::SolidCullNone));

    graphics.GetTrailRenderer()->Render(dc, rc.camera->GetView(), rc.camera->GetProjection(), color);

    dc->OMSetBlendState(renderState->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
    dc->OMSetDepthStencilState(renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
    dc->RSSetState(renderState->GetRasterizerState(RasterizerState::SolidCullBack));
}

void TrailRenderComponent::DrawGUI()
{
    ImGui::Text("Node Index : %d", nodeIndex);
    ImGui::Text("Life Time  : %.2f", lifeTime);
    ImGui::Text("Max Points : %d", maxPoints);
    ImGui::Text("Points     : %d", (int)points.size());
	ImGui::Text("Root Offset: (%.2f, %.2f, %.2f)", rootOffset.x, rootOffset.y, rootOffset.z);
	ImGui::Text("Tip Offset : (%.2f, %.2f, %.2f)", tipOffset.x, tipOffset.y, tipOffset.z);
	ImGui::Text("Tip Ratio  : %.2f", tipRatio);
	ImGui::Text("Offset Angle: (%.2f, %.2f, %.2f)", offsetAngle.x, offsetAngle.y, offsetAngle.z);

    ImGui::ColorPicker3("Color", &color.x);
}
