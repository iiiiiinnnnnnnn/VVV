// AracoreFootGrounder.h

#pragma once

#include "Component.h"
#include "Common.h"
#include "Object.h"
#include "Actor.h"
#include "ActorManager.h"
#include "Model.h"
#include "Terrain.h"
#include "GameTime.h"
#include "imgui.h"

// コライダーではなく実際のモデルの足ボーンを地形に添わせる補正。
// 足先ボーンで地面との距離を見て、脚の根元ボーン以下をまとめて上下に動かす。
class AracoreFootGrounder : public Component
{
public:
    AracoreFootGrounder(Object* owner, Model* model)
        : Component(owner), model(model)
    {
    }

    void AddLeg(const char* rootNodeName, const char* tipNodeName)
    {
        if (!model) return;

        const int rootNodeIndex = model->GetNodeIndex(rootNodeName);
        const int tipNodeIndex = model->GetNodeIndex(tipNodeName);
        if (rootNodeIndex < 0 || tipNodeIndex < 0) return;

        legs.push_back({ rootNodeIndex, tipNodeIndex });
    }

    void LateUpdate() override
    {
        if (!enabled) return;
        if (!model) return;

        Terrain* terrain = FindTerrain();
        if (!terrain) return;

        Actor* terrainActor = terrain->GetOwnerAsActor();
        Matrix terrainInverse = terrainActor->transform.matrix;
        terrainInverse.Invert();

        for (Leg& leg : legs)
        {
            FixLegToTerrain(leg, terrain, terrainActor, terrainInverse);
        }
    }

    void DrawGUI() override
    {
        if (!ImGui::TreeNode("Aracore Foot Grounder")) return;

        ImGui::Checkbox("Enabled", &enabled);
        ImGui::DragFloat("Ray Up", &rayUpDistance, 0.01f, 0.0f, 100.0f);
        ImGui::DragFloat("Ray Down", &rayDownDistance, 0.01f, 0.0f, 100.0f);
        ImGui::DragFloat("Ground Offset", &groundOffset, 0.01f, -10.0f, 10.0f);
        ImGui::DragFloat("Smooth Speed", &smoothSpeed, 0.1f, 0.1f, 60.0f);
        ImGui::Text("Legs: %d", static_cast<int>(legs.size()));

        ImGui::TreePop();
    }

private:
    struct Leg
    {
        int rootNodeIndex = -1;
        int tipNodeIndex = -1;
        float currentOffsetY = 0.0f;
    };

    Terrain* FindTerrain()
    {
        if (cachedTerrain && cachedTerrain->IsActive())
            return cachedTerrain;

        Actor* actor = GetOwnerAsActor();
        ActorManager* actorManager = actor->GetActorManager();
        if (!actorManager) return nullptr;

        for (const std::shared_ptr<Actor>& target : actorManager->GetActors())
        {
            if (!target) continue;

            Terrain* terrain = target->GetComponent<Terrain>();
            if (!terrain) continue;

            cachedTerrain = terrain;
            return cachedTerrain;
        }

        return nullptr;
    }

    void FixLegToTerrain(Leg& leg, Terrain* terrain, Actor* terrainActor, const Matrix& terrainInverse)
    {
        std::vector<Model::Node>& nodes = model->GetNodes();
        if (leg.rootNodeIndex < 0 || leg.rootNodeIndex >= static_cast<int>(nodes.size())) return;
        if (leg.tipNodeIndex < 0 || leg.tipNodeIndex >= static_cast<int>(nodes.size())) return;

        Model::Node& rootNode = nodes[leg.rootNodeIndex];
        Model::Node& tipNode = nodes[leg.tipNodeIndex];
        Vector3 tipWorldPosition = tipNode.worldTransform.Translation();
        Vector3 terrainLocalPosition = Vector3::Transform(tipWorldPosition, terrainInverse);

        const float terrainSize = terrain->GetTerrainSize();
        if (terrainSize <= eps) return;

        const float u = terrainLocalPosition.x / terrainSize + 0.5f;
        const float v = terrainLocalPosition.z / terrainSize + 0.5f;
        if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f) return;

        Vector3 terrainLocalGround = terrainLocalPosition;
        terrainLocalGround.y = terrain->GetHeightByUV(u, v);

        Vector3 terrainWorldGround = Vector3::Transform(terrainLocalGround, terrainActor->transform.matrix);
        const float fixedY = terrainWorldGround.y + groundOffset;
        const float diff = fixedY - tipWorldPosition.y;
        float targetOffsetY = 0.0f;

        if (diff <= rayUpDistance && -diff <= rayDownDistance)
            targetOffsetY = diff;

        const float rate = 1.0f - std::exp(-smoothSpeed * Game::Time::deltaTime);
        leg.currentOffsetY = std::lerp(leg.currentOffsetY, targetOffsetY, std::clamp(rate, 0.0f, 1.0f));

        if (std::abs(leg.currentOffsetY) <= eps) return;

        ApplyWorldDelta(rootNode, Vector3(0.0f, leg.currentOffsetY, 0.0f));
    }

    void ApplyWorldDelta(Model::Node& node, const Vector3& delta)
    {
        node.worldTransform._41 += delta.x;
        node.worldTransform._42 += delta.y;
        node.worldTransform._43 += delta.z;

        for (Model::Node* child : node.children)
        {
            if (!child) continue;
            ApplyWorldDelta(*child, delta);
        }
    }

    Model* model = nullptr;
    Terrain* cachedTerrain = nullptr;
    std::vector<Leg> legs;
    bool enabled = true;
    float rayUpDistance = 5.0f;
    float rayDownDistance = 50.0f;
    float groundOffset = 0.65f;
    float smoothSpeed = 18.0f;
};


