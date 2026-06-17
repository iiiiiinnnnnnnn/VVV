// Stage01.cpp

#include "Stage01.h"
#include "Graphics.h"
#include "LightManager.h"

Stage01::Stage01() : Actor("Stage01", "Stage", Layer::Stage)
{
	auto* rb = AddComponent<RigidbodyStatic>();

	Terrain* terrain = AddComponent<Terrain>();
	terrain->LoadTerrainTexture("Data/Terrain/Maps/BossField.dds");
	AddComponent<TerrainMeshCollider>(rb, 256, TerrainMeshCollider::CollisionArea{0.2844f, 0.7242f, 0.2438f, 0.6844f});

	Game::Graphics& graphics = Game::Graphics::Instance();
	graphics.GetSkyBoxRenderer()->SetIntensity(0.0f);
}

void Stage01::ApplyEnvironment(LightManager& lightManager) const
{
	DirectionalLight directionalLight{"Cave Sun", true, {0.72f, 0.82f, 1.0f, 1.0f}};
	directionalLight.transform.rotation = Quaternion::CreateFromYawPitchRoll(
		DirectX::XMConvertToRadians(-35.0f),
		DirectX::XMConvertToRadians(35.0f),
		0.0f);

	lightManager.SetDirectionalLight(directionalLight);
	lightManager.SetAmbientColor(ColorFromRGBA(0x344967FF));
}

void Stage01::OnUpdate()
{

}

void Stage01::OnDrawGUI()
{

}
