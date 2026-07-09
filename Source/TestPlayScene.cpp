// TestPlayScene.cpp

#include "TestPlayScene.h"
#include "SceneManager.h"
#include "FreeCameraController.h"
#include "ThirdPersonCameraController.h"
#include "LocalPlayer.h"
#include "Graphics.h"
#include "SpriteWidget.h"
#include "HitStop.h"
#include "Player.h"
#include "Stage01.h"

TestPlayScene::TestPlayScene(SceneMessage message) : Scene(message)
{
	ID3D11Device* device =
		Game::Graphics::Instance().GetDevice();

	float screenWidth =
		Game::Graphics::ScreenWidth;

	float screenHeight =
		Game::Graphics::ScreenHeight;

	// stage
	{
		auto stage = std::make_shared<Stage01>(&actorManager);
		stage->ApplyEnvironment(lightManager);
		actorManager.Register(stage);
	}

	// player & camera
	{
		std::shared_ptr<Player> player =
			std::make_shared<Player>();

		player->SetController(
			std::make_unique<LocalPlayer>());

		actorManager.Register(player);

		camera.SetPerspectiveFov(
			DirectX::XMConvertToRadians(45),
			screenWidth / screenHeight,
			0.1f,
			1000.0f);

		camera.SetLookAt(
			{ 0, 3, 5 },
			{ 0, 0, 0 },
			{ 0, 1, 0 });

		std::unique_ptr<ThirdPersonCameraController> third =
			std::make_unique<ThirdPersonCameraController>(
			player.get());

		player->SetCameraController(third.get());

		cameraControllers.push_back(
			std::move(third));

		cameraControllers.push_back(
			std::make_unique<FreeCameraController>(
			camera));
	}

	// test widget
	{
		auto sw = std::make_shared<SpriteWidget>("Data/Image/Test.png");
		sw->rect.size = {50.0f, 50.0f};
		sw->rect.position = {50, 50};
		sw->rect.anchor = {0.5f, 0.5f};
		sw->AddComponent<Animator>()->Load("Data/Animator/Test2D.animator");
		widgetManager.Register(sw);
	}

	//	パーティクル準備
	{
		const int num_particle = 1000;

		//	パーティクル用画像ロード
		auto sozai = Texture("Data/Image/particle256x256.png");

		//	パーティクルシステム生成
		particleSystem = std::make_unique<ParticleSystem>(
			device,
			sozai.GetShaderResourceView(),
			4, 4, num_particle);
	}
}

void TestPlayScene::OnUpdate()
{
	HitStop::Update();

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

	//	スパーク
	if (::GetAsyncKeyState('C') & 0x8000)
	{
		Vector3 pos = Vector3((rand() % 30 - 15) * 0.1f, rand() % 30 * 0.1f + 1, (rand() % 30 - 15) * 0.1f + 3);
		int max = 2;
		for (int i = 0; i < max; i++)
		{
			Vector3 p;
			p.x = pos.x;
			p.y = pos.y;
			p.z = pos.z;

			Vector3 v = { 0,0,0 };
			v.x = (rand() % 10001 - 5000) * 0.0001f;
			v.y = (rand() % 10001) * 0.0002f + 1.2f;
			v.z = (rand() % 10001 - 5000) * 0.0001f;

			Vector3 f = { 0,-1.2f,0 };
			Vector2 s = { 0.05f,0.05f };

			particleSystem->Set(2, 3, p, v, f, s);
		}
	}
	//	ボックス
	if (::GetAsyncKeyState('Z') & 0x8000)
	{
		int max = 2;
		for (int i = 0; i < max; i++)
		{
			Vector3 p;
			p.x = (rand() % 3000 - 1500) * 0.001f;
			p.y = (rand() % 3000 - 1500) * 0.001f;
			p.z = (rand() % 3000 - 1500) * 0.001f;

			Vector3 v = { 0,0,0 };

			Vector3 f = { 0,0,0 };
			Vector2 s = { 0.5f, 0.5f };

			particleSystem->Set(2, 3, p, v, f, s);
		}
	}

	particleSystem->Update();
}

void TestPlayScene::OnRender(RenderContext& rc)
{
	//	パーティクル描画
	rc.deviceContext->OMSetBlendState(rc.renderState->GetBlendState(BlendState::Additive), nullptr, 0xFFFFFFFF);
	rc.deviceContext->OMSetDepthStencilState(rc.renderState->GetDepthStencilState(DepthState::TestOnly), 0);
	rc.deviceContext->RSSetState(rc.renderState->GetRasterizerState(RasterizerState::SolidCullNone));

	particleSystem->Render(rc);
}

void TestPlayScene::OnDrawGUI()
{
	if (ImGui::CollapsingHeader("SceneManager"))
	{
		if (ImGui::Button("ReloadScene"))
		{
			SceneManager::Instance().LoadScene<TestPlayScene>();
		}
	}
}
