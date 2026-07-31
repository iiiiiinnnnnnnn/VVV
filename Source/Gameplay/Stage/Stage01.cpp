// Stage01.cpp

#include "Gameplay/Stage/Stage01.h"
#include "Rendering/Core/Graphics.h"
#include "Gameplay/Actor/ActorManager.h"
#include "Gameplay/Stage/Component/StageLoader.h"
#include "Gameplay/Camera/Camera.h"
#include "Gameplay/Camera/FreeCameraController.h"
#include "Gameplay/Actor/EnemySmall.h"

Stage01::Stage01()
{
	ID3D11Device* device = Game::Graphics::Instance().GetDevice();
	Game::Graphics& graphics = Game::Graphics::Instance();
	graphics.GetSkyBoxRenderer()->SetIntensity(0.0f);

	const bool loadedVstg = LoadVSTG("Data/Stages/0.vstg");
	_ASSERT_EXPR(loadedVstg, "Failed to load VSTG file.");

	StageLoader* stageLoader = GetComponent<StageLoader>();
	stageLoader->SetCrystalBreakParticleSystem(particleSystem.get());
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

	// 雑魚敵
	#if 1
	for (int i = 0; i < 5; i++)
	{
		auto enemy = std::make_shared<EnemySmall>(
			Vector3{-4.113f, 0.95f + (i * 10), -3.712f}, Vector3{0, 180, 0});
		actorManager.Register(enemy);
	}
	#endif
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

	// 雑魚敵
	#if 0
	//aracoreSpawnTimer -= Game::Time::deltaTime;
	if (aracoreSpawnTimer < 0)
	{
		aracoreSpawnTimer = 30.0f;

		auto aracore = std::make_shared<Aracore>(Vector3{9, 5, 10});
		aracore->SetBreakParticleSystem(particleSystem.get());
		actorManager.Register(aracore);
	}
	#endif
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

