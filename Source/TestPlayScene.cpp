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
		currentStage_ = std::make_unique<Stage01>();
	}

	Stage& stage = *currentStage_;
	ActorManager& actorManager = stage.GetActorManager();
	LightManager& lightManager = stage.GetLightManager();
	Camera& camera = *stage.GetCamera();
	auto& cameraControllers = stage.GetCameraControllers();

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
}

void TestPlayScene::OnUpdate()
{
	HitStop::Update();
}

void TestPlayScene::OnRender(RenderContext& rc)
{

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
