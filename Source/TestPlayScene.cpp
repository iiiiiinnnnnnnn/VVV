// TestPlayScene.cpp

#include "TestPlayScene.h"
#include "SceneManager.h"

// camera
#include "FreeCameraController.h"
#include "ThirdPersonCameraController.h"

// player control
#include "LocalPlayer.h"

// graphics
#include "SceneEffect.h"
#include "Graphics.h"
#include "SpriteWidget.h"

// actor
#include "Player.h"
#include "Stage00.h"
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
		auto stage = std::make_shared<Stage01>();
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

	DamageVignette::Init(device);
}

void TestPlayScene::OnUpdate()
{
	HitStop::Update();

	DamageVignette::Update(
		Game::Graphics::Instance().GetSpriteRenderer(),
		Game::Graphics::ScreenWidth,
		Game::Graphics::ScreenHeight);
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
