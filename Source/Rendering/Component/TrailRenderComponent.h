// TrailRenderComponent.h
#pragma once
#include <deque>

#include "Core/Object/Component.h"
#include "Resource/VMDLModel.h"

class TrailRenderComponent : public Component
{
public:
    TrailRenderComponent(
        Object* owner,
        VMDLModel* model,
        int   nodeIndex,
        Color color = {1.0f, 0.9f, 0.3f, 1.0f},
        float rootOffset = 0.0f,
        float tipOffset = -1.0f,
        float tipRatio = 1.0f,
        float lifeTime   = 0.5f,
        int   maxPoints  = 40);
	TrailRenderComponent(
		Object* owner,
		VMDLModel* model,
		int nodeIndex,
		const Vector3& rootOffset,
		const Vector3& tipOffset,
		Color color,
		float tipRatio = 1.0f,
		float lifeTime = 0.5f,
		int maxPoints = 40);

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
        float   age;
    };
    const std::deque<TrailPoint>& GetPoints() const { return points; }
    float GetLifeTime() const { return lifeTime; }

    void StartTrail() { stopping = false; }
	void StopTrail() { stopping = true; }

private:
    void BuildTrailVertices();

    VMDLModel* model;
    int   nodeIndex;
	Vector3 rootOffset;
	Vector3 tipOffset;
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
