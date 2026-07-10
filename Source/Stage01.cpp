// Stage01.cpp

#include "Stage01.h"
#include "Graphics.h"
#include "LightManager.h"
#include "ActorManager.h"
#include "Aracore.h"
#include "AracoreQueen.h"
#include "NavMeshActor.h"
#include "StageLoader.h"
#include "Rigidbody.h"
#include "Terrain.h"
#include "TerrainMeshCollider.h"
#include "GameTime.h"

Stage01::Stage01(ActorManager* actorManager) : Actor("Stage01", "Stage", true)
{
	this->actorManager = actorManager;

	auto* rb = AddComponent<RigidbodyStatic>();

	Terrain* terrain = AddComponent<Terrain>();
	terrain->LoadTerrainTexture("Data/Terrain/Maps/test.dds");
	AddComponent<TerrainMeshCollider>(Layers::Get("Terrain"), rb,
		TerrainMeshCollider::CollisionArea{0.34f, 0.664f, 0.304f, 0.624f});
	AddComponent<StageLoader>(this, "Data/Stages/Stage01.json");
	AddComponent<NavMeshActor>();

	Game::Graphics& graphics = Game::Graphics::Instance();
	graphics.GetSkyBoxRenderer()->SetIntensity(0.0f);

	// ƒ{ƒX“G
	#if 0
	auto boss = std::make_shared<AracoreQueen>();
	actorManager->Register(boss);
	#endif
}

void Stage01::ApplyEnvironment(LightManager& lightManager) const
{
	DirectionalLight directionalLight{"Cave Sun", "Cave Sun", true, {1.0f, 1.0f, 1.0f, 1.0f}};
	directionalLight.transform.rotation = Quaternion::CreateFromYawPitchRoll(
		DirectX::XMConvertToRadians(-35.0f),
		DirectX::XMConvertToRadians(35.0f),
		0.0f);

	lightManager.SetDirectionalLight(directionalLight);
	lightManager.SetAmbientColor(ColorFromRGBA(0x2A4C7DFF));
}

void Stage01::SetCrystalBreakParticleSystem(ParticleSystem* particleSystem)
{
	if (StageLoader* stageLoader = GetComponent<StageLoader>())
	{
		stageLoader->SetCrystalBreakParticleSystem(particleSystem);
	}
}

void Stage01::OnUpdate()
{
	//aracoreSpawnTimer -= Game::Time::deltaTime;
	if (aracoreSpawnTimer < 0)
	{
		aracoreSpawnTimer = 30.0f;

		auto aracore = std::make_shared<Aracore>();
		aracore->transform.SetPosition(-6, 5, 6);
		actorManager->Register(aracore);
	}
}

void Stage01::OnDrawGUI()
{

}

