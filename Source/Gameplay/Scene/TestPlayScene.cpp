// TestPlayScene.cpp

#include "Gameplay/Scene/TestPlayScene.h"
#include "Gameplay/Scene/SceneManager.h"
#include "Gameplay/Camera/FreeCameraController.h"
#include "Gameplay/Camera/ThirdPersonCameraController.h"
#include "Gameplay/Player/LocalPlayer.h"
#include "Rendering/Core/Graphics.h"
#include "UI/SpriteWidget.h"
#include "Gameplay/Scene/HitStop.h"
#include "Gameplay/Player/Player.h"
#include "Gameplay/Stage/Stage01.h"

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
		currentStage = std::make_unique<Stage01>();
	}

	Stage& stage = *currentStage;
	ActorManager& actorManager = stage.GetActorManager();
	LightManager& lightManager = stage.GetLightManager();
	Camera* camera = stage.GetActiveCamera();
	Actor* cameraActor = stage.GetDefaultCameraActor();

	// player & camera
	{
		std::shared_ptr<Player> player =
			std::make_shared<Player>();

		player->SetController(
			std::make_unique<LocalPlayer>());

		actorManager.Register(player);

		camera->SetPerspectiveFov(
			DirectX::XMConvertToRadians(45),
			screenWidth / screenHeight,
			0.1f,
			1000.0f);

		camera->SetLookAt(
			{ 0, 3, 5 },
			{ 0, 0, 0 },
			{ 0, 1, 0 });

		ThirdPersonCameraController* third =
			cameraActor->AddComponent<ThirdPersonCameraController>(player.get());

		player->SetCameraController(third);

		FreeCameraController* freeCamera =
			cameraActor->AddComponent<FreeCameraController>();
		freeCamera->SetActive(false);
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
