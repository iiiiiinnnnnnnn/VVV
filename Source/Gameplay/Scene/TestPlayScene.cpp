// TestPlayScene.cpp

#include "Gameplay/Scene/TestPlayScene.h"
#include "Gameplay/Scene/GameStartScene.h"
#include "Gameplay/Scene/SceneManager.h"
#include "Gameplay/Camera/FreeCameraController.h"
#include "Gameplay/Camera/ThirdPersonCameraController.h"
#include "Gameplay/Player/LocalPlayer.h"
#include "Rendering/Core/Graphics.h"
#include "UI/SpriteWidget.h"
#include "Gameplay/Scene/HitStop.h"
#include "Gameplay/Player/Player.h"
#include "Gameplay/Stage/Stage01.h"

namespace
{
	void RestoreMouseCursor()
	{
		Mouse& mouse = Game::Input::Instance().GetMouse();
		mouse.SetCursorLock(false);
		mouse.SetCursorVisible(true);
	}
}

TestPlayScene::TestPlayScene(SceneMessage message) : Scene(message)
{
	Game::Graphics& graphics = Game::Graphics::Instance();
	graphics.SetBorderlessFullscreen(true);
	graphics.SetWindowMovementLocked(true);

	ID3D11Device* device =
		graphics.GetDevice();

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
		std::shared_ptr<LocalPlayer> player =
			std::make_shared<LocalPlayer>();

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

TestPlayScene::~TestPlayScene()
{
	RestoreMouseCursor();
	Game::Graphics::Instance().SetWindowMovementLocked(false);
}

void TestPlayScene::OnUpdate()
{
	if (Game::Input::Instance().GetGamePad().GetButtonDown() & GamePad::BTN_ESCAPE)
	{
		RestoreMouseCursor();
		SceneManager::Instance().LoadScene<GameStartScene>();
		return;
	}

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
