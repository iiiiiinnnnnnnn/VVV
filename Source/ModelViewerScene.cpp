// ModelViewerScene.cpp

#include "ModelViewerScene.h"
#include "Commander.h"
#include "Soldier.h"
#include "Stage00.h"
#include "FreeCameraController.h"
#include "FpsCameraController.h"
#include "LocalPlayer.h"

// コンストラクタ
ModelViewerScene::ModelViewerScene()
{
	ID3D11Device* device = Graphics::Instance().GetDevice();
	float screenWidth = Graphics::Instance().GetScreenWidth();
	float screenHeight = Graphics::Instance().GetScreenHeight();

	// アクタ生成

	actors.Register(std::make_shared<Stage00>());

	std::shared_ptr<Commander> localCommander = std::make_shared<Commander>();
	localCommander->SetController(std::make_unique<LocalPlayer>());
	actors.Register(localCommander);

	// カメラ設定
	camera.SetPerspectiveFov(
		DirectX::XMConvertToRadians(45),	// 画角
		screenWidth / screenHeight,			// 画面アスペクト比
		0.1f,								// ニアクリップ
		1000.0f								// ファークリップ
	);
	camera.SetLookAt(
		{ 0, 3, 5 },		// 視点
		{ 0, 0, 0 },		// 注視点
		{ 0, 1, 0 }			// 上ベクトル
	);

	// カメラからコントローラー0生成
	cameraControllers.push_back(std::make_unique<FreeCameraController>(camera));

	// コントローラー1生成
	cameraControllers.push_back(std::make_unique<FpsCameraController>(localCommander));
}

// 更新処理
void ModelViewerScene::OnUpdate(float elapsedTime)
{

}

// 描画処理
void ModelViewerScene::OnRender(RenderContext& rc, float elapsedTime)
{

}

// GUI描画処理
void ModelViewerScene::OnDrawGUI(float elapsedTime)
{

}
