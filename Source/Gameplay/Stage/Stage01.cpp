// Stage01.cpp

#include "Gameplay/Stage/Stage01.h"
#include "Rendering/Core/Graphics.h"
#include "Gameplay/Actor/ActorManager.h"
#include "Gameplay/Camera/Camera.h"
#include "Gameplay/Camera/FreeCameraController.h"
#include "Gameplay/Actor/EnemySmall.h"
#include "Application/Time/GameTime.h"
#include "Gameplay/Actor/AracoreQueen.h"

Stage01::Stage01()
{
	ID3D11Device* device = Game::Graphics::Instance().GetDevice();
	Game::Graphics& graphics = Game::Graphics::Instance();
	graphics.GetSkyBoxRenderer()->SetIntensity(0.0f);

	const bool loadedVstg = LoadVSTG("Data/Stages/0_tall.vstg");
	_ASSERT_EXPR(loadedVstg, "Failed to load VSTG file.");

	stageLoader = GetComponent<StageLoader>();
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

	{
		Texture fogTexture("Data/Image/fog_particle.png");
		fogParticleSystem = std::make_unique<ParticleSystem>(
			device,
			fogTexture.GetShaderResourceView(),
			1,
			1,
			256);
	}

	// 登録したスポナー動作させるためのファクトリ関数
	#if 1
	stageLoader->RegisterSpawnerFactory("EnemySmall", [](const Transform& transform)
	{
		auto enemy = std::make_shared<EnemySmall>(transform.position);
		enemy->transform.SetRotation(transform.rotation);
		enemy->transform.SetScale(transform.scale);
		return enemy;
	});
	#endif
	#if 1
	stageLoader->RegisterSpawnerFactory("EnemyBig", [](const Transform& transform)
	{
		auto enemy = std::make_shared<AracoreQueen>(transform.position);
		enemy->transform.SetRotation(transform.rotation);
		enemy->transform.SetScale(transform.scale);
		return enemy;
	});
	#endif
	stageLoader->SpawnEntities();
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
		Vector2 s = { 0.2f,0.2f };

		particleSystem->Set(12, 5, p, v, f, s);
	}

	particleSystem->Update();

	// 煙

	if (!fogParticleSystem) return;
	if (!fogPrewarmed)
	{
		for (int i = 0; i < 96; ++i) SpawnFogParticle();
		fogPrewarmed = true;
	}

	fogSpawnAccumulator += fogSpawnRate * Game::Time::deltaTime;
	while (fogSpawnAccumulator >= 1.0f)
	{
		SpawnFogParticle();
		fogSpawnAccumulator -= 1.0f;
	}
	fogParticleSystem->Update();
}

void Stage01::RenderEffects(const RenderContext& rc)
{
	// 降雪
	rc.deviceContext->OMSetBlendState(rc.renderState->GetBlendState(BlendState::Additive), nullptr, 0xFFFFFFFF);
	rc.deviceContext->OMSetDepthStencilState(rc.renderState->GetDepthStencilState(DepthState::TestOnly), 0);
	rc.deviceContext->RSSetState(rc.renderState->GetRasterizerState(RasterizerState::SolidCullNone));
	particleSystem->Render(rc);

	// 霧
	if (!fogParticleSystem) return;
	rc.deviceContext->OMSetBlendState(
		rc.renderState->GetBlendState(BlendState::Transparency),
		nullptr,
		0xFFFFFFFF);
	fogParticleSystem->Render(rc);
}

void Stage01::OnDrawGUI()
{
	if (!ImGui::TreeNode("Fog")) return;
	ImGui::DragFloat("Spawn Rate", &fogSpawnRate, 0.1f, 0.0f, 30.0f);
	ImGui::DragFloat("Area Half Width", &fogAreaHalfWidth, 0.5f, 1.0f, 500.0f);
	ImGui::DragFloat("Area Half Depth", &fogAreaHalfDepth, 0.5f, 1.0f, 500.0f);
	ImGui::DragFloat("Min Height", &fogMinHeight, 0.1f, -10.0f, fogMaxHeight);
	ImGui::DragFloat("Max Height", &fogMaxHeight, 0.1f, fogMinHeight, 20.0f);
	ImGui::DragFloat("Min Lifetime", &fogMinLifetime, 0.1f, 0.1f, fogMaxLifetime);
	ImGui::DragFloat("Max Lifetime", &fogMaxLifetime, 0.1f, fogMinLifetime, 30.0f);
	ImGui::DragFloat("Fade In", &fogFadeInDuration, 0.1f, 0.0f, fogMinLifetime);
	ImGui::DragFloat("Fade Out", &fogFadeOutDuration, 0.1f, 0.0f, fogMinLifetime);
	ImGui::DragFloat("Min Size", &fogMinSize, 0.1f, 0.1f, fogMaxSize);
	ImGui::DragFloat("Max Size", &fogMaxSize, 0.1f, fogMinSize, 30.0f);
	ImGui::DragFloat("Drift Speed", &fogDriftSpeed, 0.01f, 0.0f, 5.0f);
	ImGui::ColorEdit4("Fog Color", &fogColor.x);
	ImGui::TreePop();
}

void Stage01::SpawnFogParticle()
{
	if (!fogParticleSystem) return;

	const Vector3 position = transform.position + Vector3(
		Random::Range(-fogAreaHalfWidth, fogAreaHalfWidth),
		Random::Range(fogMinHeight, fogMaxHeight),
		Random::Range(-fogAreaHalfDepth, fogAreaHalfDepth));
	const float driftAngle = Random::Range(0.0f, DirectX::XM_2PI);
	const Vector3 velocity = {
		cosf(driftAngle) * fogDriftSpeed,
		Random::Range(0.0f, fogDriftSpeed * 0.25f),
		sinf(driftAngle) * fogDriftSpeed};
	const float size = Random::Range(fogMinSize, fogMaxSize);
	const Vector2 particleSize = {
		size * Random::Range(0.8f, 1.2f),
		size * Random::Range(0.55f, 0.9f)};

	fogParticleSystem->Set(
		0,
		Random::Range(fogMinLifetime, fogMaxLifetime),
		position,
		velocity,
		Vector3::Zero,
		particleSize,
		false,
		0.0f,
		fogColor,
		fogFadeInDuration,
		fogFadeOutDuration);
}
