// Stage01.cpp

#include "Gameplay/Stage/Stage01.h"
#include "Rendering/Core/Graphics.h"
#include "Gameplay/Lighting/LightManager.h"
#include "Gameplay/Actor/ActorManager.h"
#include "Gameplay/Actor/Aracore.h"
#include "Gameplay/Actor/AracoreQueen.h"
#include "Physics/Navigation/NavMeshActor.h"
#include "Gameplay/Stage/Component/StageLoader.h"
#include "Physics/RigidBody/Rigidbody.h"
#include "Gameplay/Stage/Component/Terrain.h"
#include "Physics/Collider/TerrainMeshCollider.h"
#include "Gameplay/Camera/Camera.h"
#include "Gameplay/Camera/FreeCameraController.h"

Stage01::Stage01()
{
	ID3D11Device* device = Game::Graphics::Instance().GetDevice();
	Game::Graphics& graphics = Game::Graphics::Instance();
	graphics.GetSkyBoxRenderer()->SetIntensity(0.0f);

	auto* rb = AddComponent<RigidbodyStatic>();

	Terrain* terrain = AddComponent<Terrain>();
	terrain->LoadTerrainTexture("Data/Terrain/Maps/test.dds");
	AddComponent<TerrainMeshCollider>(Layers::Get("Terrain"), rb,
		TerrainMeshCollider::CollisionArea{0.34f, 0.664f, 0.304f, 0.624f});
	StageLoader* stageLoader = AddComponent<StageLoader>(this, "Data/Stages/Stage01.json");
	stageLoader->SetCrystalBreakParticleSystem(particleSystem.get());
	AddComponent<NavMeshActor>();

	// F4縺ｧ蛻・ｊ譖ｿ縺医ｋ繧ｹ繝・・繧ｸ遒ｺ隱咲畑繧ｫ繝｡繝ｩ
	{
		auto debugCameraActor = std::make_shared<Actor>("Debug Camera");
		Camera* debugCamera = debugCameraActor->AddComponent<Camera>(100);
		debugCamera->SetPerspectiveFov(
			DirectX::XMConvertToRadians(45.0f),
			Game::Graphics::ScreenWidth / Game::Graphics::ScreenHeight,
			0.1f,
			1000.0f);
		debugCamera->SetLookAt({0.0f, 3.0f, 5.0f}, Vector3::Zero, Vector3::Up);

		FreeCameraController* controller =
			debugCameraActor->AddComponent<FreeCameraController>();
		debugCamera->SetActive(false);
		controller->SetActive(false);

		SetDebugCamera(debugCamera);
		actorManager.Register(debugCameraActor);
	}

	//	パーティクル準備
	{
		//	パーティクル用画像ロード
		auto sozai = Texture("Data/Image/particle256x256.png");

		//	パーティクルシステム生成
		particleSystem = std::make_unique<ParticleSystem>(
			device,
			sozai.GetShaderResourceView(),
			4, 4, 1000);
		stageLoader->SetCrystalBreakParticleSystem(particleSystem.get());
	}

	// ボス敵
	#if 0
	auto boss = std::make_shared<AracoreQueen>();
	actorManager.Register(boss);
	#endif


	DirectionalLight directionalLight{"Cave Sun", "Cave Sun", true, {1.0f, 1.0f, 1.0f, 1.0f}};
	directionalLight.transform.rotation = Quaternion::CreateFromYawPitchRoll(
		RAD(-35.0f), RAD(35.0f), 0.0f);

	lightManager.SetDirectionalLight(directionalLight);
	lightManager.SetAmbientColor(ColorFromRGBA(0x2A4C7DFF));
}

void Stage01::OnUpdate()
{
	//	降雪
	Vector3 pos = Vector3((rand() % 30 - 15) * 0.1f, rand() % 30 * 0.1f + 1, (rand() % 30 - 15) * 0.1f + 3);
	int max = 2;
	for (int i = 0; i < max; i++)
	{
		//	発生位置
		Vector3 p = { 0,0,0 };
		p.x = pos.x + (rand() % 10001 - 5000) * 0.01f;
		p.y = pos.y;
		p.z = pos.z + (rand() % 10001 - 5000) * 0.01f;
		//	発生方向
		Vector3 v = { 0,0,0 };
		v.y = -(rand() % 10001) * 0.0002f - 0.002f;
		//	力
		Vector3 f = { 0,0,0 };
		f.x = (rand() % 10001) * 0.00001f + 0.1f;
		f.z = (rand() % 10001 - 5000) * 0.00001f;
		//	大きさ
		Vector2 s = { .2f,.2f };

		particleSystem->Set(12, 5, p, v, f, s);
	}

	particleSystem->Update();

	//aracoreSpawnTimer -= Game::Time::deltaTime;
	if (aracoreSpawnTimer < 0)
	{
		aracoreSpawnTimer = 30.0f;

		auto aracore = std::make_shared<Aracore>(Vector3{9, 5, 10});
		aracore->SetBreakParticleSystem(particleSystem.get());
		actorManager.Register(aracore);
	}
}

void Stage01::OnRender(const RenderContext& rc)
{
	//	パーティクル描画
	rc.deviceContext->OMSetBlendState(rc.renderState->GetBlendState(BlendState::Additive), nullptr, 0xFFFFFFFF);
	rc.deviceContext->OMSetDepthStencilState(rc.renderState->GetDepthStencilState(DepthState::TestOnly), 0);
	rc.deviceContext->RSSetState(rc.renderState->GetRasterizerState(RasterizerState::SolidCullNone));

	particleSystem->Render(rc);
}

void Stage01::OnDrawGUI()
{
	
}

