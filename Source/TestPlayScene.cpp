// TestPlayScene.cpp

#include "TestPlayScene.h"
#include "Commander.h"
#include "Soldier.h"
#include "Stage00.h"
#include "FreeCameraController.h"
#include "FpsCameraController.h"
#include "LocalPlayer.h"
#include "TestWIdget.h"

TestPlayScene::TestPlayScene()
{
	ID3D11Device* device = Game::Graphics::Instance().GetDevice();
	float screenWidth = Game::Graphics::ScreenWidth;
	float screenHeight = Game::Graphics::ScreenHeight;

	actors.Register(std::make_shared<Stage00>());

	// ステージGLBからライトを読み込む
	#if 0
	GLTFImporter importer("Data/Model/Church_cathedral.glb");
	std::vector<Model::Node> nodes;
	importer.LoadNodes(nodes);
	importer.LoadLights(lightData, nodes);
	#endif

	std::shared_ptr<Commander> localCommander = std::make_shared<Commander>();
	localCommander->SetController(std::make_unique<LocalPlayer>());
	actors.Register(localCommander);

	widgets.Register(std::make_shared<TestWidget>(Vector2(0, 0)));
	widgets.Register(std::make_shared<TestWidget>(Vector2(0, 100)));

	camera.SetPerspectiveFov(
		DirectX::XMConvertToRadians(45),
		screenWidth / screenHeight,
		0.1f,
		1000.0f
	);
	camera.SetLookAt({0, 3, 5}, {0, 0, 0}, {0, 1, 0});

	cameraControllers.push_back(std::make_unique<FreeCameraController>(camera));
	cameraControllers.push_back(std::make_unique<FpsCameraController>(localCommander));
}

void TestPlayScene::OnUpdate()
{
}

void TestPlayScene::OnDrawGUI()
{

}
