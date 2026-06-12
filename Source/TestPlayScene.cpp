// TestPlayScene.cpp

#include "TestPlayScene.h"

// actor
#include "Player.h"
#include "Apple.h"
#include "Stage00.h"
#include "Stage01.h"

// widget
#include "TestWidget.h"

// camera
#include "FreeCameraController.h"
#include "ThirdPersonCameraController.h"

// player controll
#include "LocalPlayer.h"

#include "HitEffect.h"
#include "Graphics.h"
#include "ResourceManager.h"

TestPlayScene::TestPlayScene()
{
    ID3D11Device* device = Game::Graphics::Instance().GetDevice();
    float screenWidth  = Game::Graphics::ScreenWidth;
    float screenHeight = Game::Graphics::ScreenHeight;
    
    // stage
    {
        actors.Register(std::make_shared<Stage01>());
    }

    // player & camera
    {
        std::shared_ptr<Player> player = std::make_shared<Player>();
        player->SetController(std::make_unique<LocalPlayer>());
        actors.Register(player);

        // camera
        camera.SetPerspectiveFov(
            DirectX::XMConvertToRadians(45),
            screenWidth / screenHeight,
            0.1f,
            1000.0f
        );
        camera.SetLookAt({0, 3, 5}, {0, 0, 0}, {0, 1, 0});

        std::unique_ptr<ThirdPersonCameraController> third = std::make_unique<ThirdPersonCameraController>(player);
        player->SetCameraController(third.get());
        cameraControllers.push_back(std::move(third));

        cameraControllers.push_back(std::make_unique<FreeCameraController>(camera));
    }

    // Apple(normal)
    {
        auto apple = std::make_shared<Apple>();
		apple->SetAggressive(false);
        apple->SetPosition({8, 0, 8});
        actors.Register(apple);
    }

    // Apple(aggressive)
    {
        auto apple = std::make_shared<Apple>();
        apple->SetAggressive(true);
        apple->SetPosition({-8, 0, 8});
        actors.Register(apple);
    }

    // widget
    {
        widgets.Register(std::make_shared<TestWidget>(Vector2(0, 0)));
        widgets.Register(std::make_shared<TestWidget>(Vector2(0, 100)));
    }

	DamageVignette::Init(device);
}

void TestPlayScene::OnUpdate()
{
    HitStop::Update();

	DamageVignette::Update(
        Game::Graphics::Instance().GetSpriteRenderer(),
        Game::Graphics::ScreenWidth, Game::Graphics::ScreenHeight);
}

void TestPlayScene::OnDrawGUI()
{
}
