// Stage01.cpp

#include "Stage01.h"

Stage01::Stage01() : Actor("Stage01", "Stage", Layer::Stage)
{
    auto* rb = AddComponent<RigidbodyStatic>();

    AddComponent<Terrain>();
    AddComponent<TerrainMeshCollider>(rb, 1024);
}

void Stage01::OnUpdate()
{

}

void Stage01::OnDrawGUI()
{

}
