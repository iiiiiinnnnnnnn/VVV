// TrailRenderComponent.h
#pragma once

#include "Component.h"
#include "Model.h"

class TrailRenderComponent : public Component
{
public:
    // model        : ノード位置の取得元
    // nodeIndex    : 追跡するボーンのインデックス
    // rootOffset   : ノード原点からのroot側オフセット（ローカルX）
    // tipOffset    : ノード原点からのtip側オフセット（ローカルX）
    // lifeTime     : 点が消えるまでの秒数
    // maxPoints    : 同時に保持する点の上限
    TrailRenderComponent(
        Object* owner,
        Model* model,
        int   nodeIndex,
        Color color = {1.0f, 0.9f, 0.3f, 1.0f},
        float rootOffset = 0.0f,
        float tipOffset = -1.0f,
        float tipRatio = 1.0f,
        float lifeTime   = 0.5f,
        int   maxPoints  = 40);

    void LateUpdate() override;
    void Render(const RenderContext& rc);
    void RenderTrail(const RenderContext& rc);
    void DrawGUI() override;
	const char* GetDebugName() const override { return ICON_FA_MAGIC " TrailRenderComponent"; }

    struct TrailPoint
    {
        Vector3 root;
        Vector3 tip;
        Vector3 tipFull;
        float   age; // 0.0 = 新しい, lifeTime = 消える
    };
    const std::deque<TrailPoint>& GetPoints() const { return points; }
    float GetLifeTime() const { return lifeTime; }

    void StartTrail() { stopping = false; }
	void StopTrail() { stopping = true; }

private:
    void BuildTrailVertices();

    Model* model;
    int   nodeIndex;
    float rootOffset;   // ローカルX方向のroot位置
    float tipOffset;    // ローカルX方向のtip位置
    float tipRatio;
    float lifeTime;
    int   maxPoints;

    // trail cbuffer data
    Color color;

    bool stopping = false;

    std::deque<TrailPoint> points;

    static constexpr float SAMPLE_INTERVAL = 0.01f;
    float sampleTimer = 0.0f;
};

