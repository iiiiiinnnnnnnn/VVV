// Stage01.cpp

#include "Stage01.h"

Stage01::Stage01() : Actor("Stage01", "Stage", Layer::Stage)
{
	auto* rb = AddComponent<RigidbodyStatic>();

	Terrain* terrain = AddComponent<Terrain>();
	terrain->LoadTerrainTexture("Data/Terrain/Maps/BossField.dds");
	AddComponent<TerrainMeshCollider>(rb, 256, TerrainMeshCollider::CollisionArea{0.2844f, 0.7242f, 0.2438f, 0.6844f});
}

void Stage01::OnUpdate()
{

}

void Stage01::OnDrawGUI()
{

}
