// Stage01.cpp

#include "Stage01.h"

Stage01::Stage01() : Actor("Stage01", "Stage", Layer::Stage)
{
    auto* rb = AddComponent<RigidbodyStatic>();

    Terrain* terrain = AddComponent<Terrain>();
	terrain->LoadTerrainTexture("Data/Terrain/Maps/BossField.dds");
    AddComponent<TerrainMeshCollider>(rb, 1024, TerrainMeshCollider::CollisionArea{-82.8f, 87.1f, -103.1f, 62.2f});
}

void Stage01::OnUpdate()
{

}

void Stage01::OnDrawGUI()
{

}
