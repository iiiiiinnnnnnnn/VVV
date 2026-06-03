// TestPlayScene.cpp

#include "TestPlayScene.h"

// actor
#include "Player.h"
#include "Stage00.h"
#include "Enemy.h"

// widget
#include "TestWidget.h"

// camera
#include "FreeCameraController.h"
#include "ThirdPersonCameraController.h"

// player controll
#include "LocalPlayer.h"

TestPlayScene::TestPlayScene()
{
    ID3D11Device* device = Game::Graphics::Instance().GetDevice();
    float screenWidth  = Game::Graphics::ScreenWidth;
    float screenHeight = Game::Graphics::ScreenHeight;

    // stage
    actors.Register(std::make_shared<Stage00>());

    // player
    std::shared_ptr<Player> player = std::make_shared<Player>();
    player->SetController(std::make_unique<LocalPlayer>());
    actors.Register(player);

    // enem
    actors.Register(std::make_shared<Enemy>());

    // widget
    widgets.Register(std::make_shared<TestWidget>(Vector2(0, 0)));
    widgets.Register(std::make_shared<TestWidget>(Vector2(0, 100)));

    // camera
    camera.SetPerspectiveFov(
        DirectX::XMConvertToRadians(45),
        screenWidth / screenHeight,
        0.1f,
        1000.0f
    );
    camera.SetLookAt({0, 3, 5}, {0, 0, 0}, {0, 1, 0});

    cameraControllers.push_back(std::make_unique<FreeCameraController>(camera));

	std::unique_ptr<ThirdPersonCameraController> third = std::make_unique<ThirdPersonCameraController>(player);
    player->SetCameraController(third.get());
	cameraControllers.push_back(std::move(third));
}

void TestPlayScene::OnUpdate()
{
}

void TestPlayScene::OnDrawGUI()
{
}
