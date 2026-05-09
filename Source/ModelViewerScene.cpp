// ModelViewerScene.cpp

#include "ModelViewerScene.h"
#include "Player.h"
#include "Stage00.h"
#include "Weapon.h"
#include "FreeCameraController.h"
#include "FpsCameraController.h"

// コンストラクタ
ModelViewerScene::ModelViewerScene()
{
	ID3D11Device* device = Graphics::Instance().GetDevice();
	float screenWidth = Graphics::Instance().GetScreenWidth();
	float screenHeight = Graphics::Instance().GetScreenHeight();

	// アクタ生成
	actors.push_back(std::make_shared<Stage00>());
	auto pl = std::make_shared<Player>();
	auto wp = std::make_shared<Weapon>(pl.get());
	pl->SetWeapon(wp.get());
	actors.push_back(pl);
	actors.push_back(wp);

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
	cameraControllers.push_back(std::make_unique<FpsCameraController>(pl));
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

//// プロパティGUI描画
//void ModelViewerScene::DrawPropertyGUI()
//{
//	if (ImGui::Begin("Property", nullptr, ImGuiWindowFlags_None))
//	{
//		for (int i = 0; i < debug_models.size(); i++)
//		{
//			Model* debug_model = debug_models[i];
//			if (debug_model)
//			{
//				ImGui::PushID(debug_model);
//
//				if (selectionNode != nullptr)
//				{
//					if (ImGui::CollapsingHeader("Node", ImGuiTreeNodeFlags_DefaultOpen))
//					{
//						// 位置
//						if (ImGui::DragFloat3("Local Position", &selectionNode->position.x, 0.1f))
//						{
//							//animationPlaying = false;
//						}
//
//						Vector3 globalPos, globalSca;
//						Quaternion globalRot;
//						selectionNode->globalTransform.Decompose(globalPos, globalRot, globalSca);
//						Vector3 globalRotEuler = globalRot.ToEuler();
//
//						// 位置
//						if (ImGui::DragFloat3("Global Position", &globalPos.x, 0.1f))
//						{
//							if (selectionNode->parent != nullptr)
//							{
//								DirectX::XMMATRIX ParentGlobalTransform = selectionNode->parent->globalTransform;
//								DirectX::XMMATRIX InverseParentGlobalTransform = DirectX::XMMatrixInverse(nullptr, ParentGlobalTransform);
//								DirectX::XMVECTOR LocalPosition = Vector3::Transform(globalPos, InverseParentGlobalTransform);
//								selectionNode->position = LocalPosition;
//							}
//							else
//							{
//								selectionNode->position = globalPos;
//							}
//							//animationPlaying = false;
//						}
//
//						// 回転
//						Vector3 angle = selectionNode->rotation.ToEuler();
//						if (ImGui::DragFloat3("Local Rotation", &angle.x, 1.0f))
//						{
//							selectionNode->rotation = Quaternion::CreateFromYawPitchRoll(angle.y, angle.x, angle.z);
//						}
//
//						if (ImGui::DragFloat3("Global Rotation", &globalRotEuler.x, 0.1f))
//						{
//							Quaternion GlobalRotation = Quaternion::CreateFromYawPitchRoll(globalRotEuler.y, globalRotEuler.x, globalRotEuler.z);
//
//							if (selectionNode->parent != nullptr)
//							{
//								DirectX::XMMATRIX ParentGlobalTransform = selectionNode->parent->globalTransform;
//								ParentGlobalTransform.r[0] = DirectX::XMVector3Normalize(ParentGlobalTransform.r[0]);
//								ParentGlobalTransform.r[1] = DirectX::XMVector3Normalize(ParentGlobalTransform.r[1]);
//								ParentGlobalTransform.r[2] = DirectX::XMVector3Normalize(ParentGlobalTransform.r[2]);
//								DirectX::XMVECTOR ParentGlobalRotation = DirectX::XMQuaternionRotationMatrix(ParentGlobalTransform);
//								DirectX::XMVECTOR InverseParentGlobalRotation = DirectX::XMQuaternionInverse(ParentGlobalRotation);
//								DirectX::XMVECTOR LocalRotation = DirectX::XMQuaternionMultiply(GlobalRotation, InverseParentGlobalRotation);
//								selectionNode->rotation = LocalRotation;
//							}
//							else
//							{
//								selectionNode->rotation = GlobalRotation;
//							}
//							//animationPlaying = false;
//						}
//
//						// スケール
//						if (ImGui::DragFloat3("Local Scale", &selectionNode->scale.x, 0.01f))
//						{
//							//animationPlaying = false;
//						}
//
//						if (ImGui::DragFloat3("Global Scale", &globalSca.x, 0.1f))
//						{
//						}
//
//					}
//				}
//
//				ImGui::PopID();
//			}
//		}
//	}
//
//	ImGui::End();
//}
//
//// アニメーションGUI描画
//void ModelViewerScene::DrawAnimationGUI()
//{
//	//if (ImGui::Begin("Animation", nullptr, ImGuiWindowFlags_None))
//	//{
//	//	ImGui::Checkbox("Loop", &animationLoop); ImGui::SameLine();
//	//	ImGui::SetNextItemWidth(70);
//	//	ImGui::InputFloat("SamplingRate", &animationSamplingRate);
//	//	ImGui::DragFloat("BlendSeconds", &animationBlendSeconds, 0.01f);
//
//	//	if (model != nullptr)
//	//	{
//	//		float secondsLength = currentAnimationIndex >= 0 ? model->GetAnimations().at(currentAnimationIndex).secondsLength : 0;
//	//		int currentFrame = static_cast<int>(currentAnimationSeconds * 60.0f);
//	//		int frameLength = static_cast<int>(secondsLength * 60);
//
//	//		ImGui::SetNextItemWidth(50);
//	//		ImGui::PushID(u8"フレーム");
//	//		if (ImGui::DragInt("##v", &currentFrame, 1, 0, frameLength))
//	//		{
//	//			animationPlaying = true;
//	//			currentAnimationSeconds = currentFrame / 60.0f;
//	//			animationSpeed = 0.0f;
//	//		}
//	//		ImGui::PopID();
//
//	//		ImGui::SameLine();
//	//		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
//	//		ImGui::PushID(u8"タイムライン");
//	//		if (ImGui::SliderFloat("##v", &currentAnimationSeconds, 0, secondsLength, "%.3f"))
//	//		{
//	//			animationPlaying = true;
//	//			animationSpeed = 0.0f;
//	//		}
//	//		ImGui::PopID();
//
//	//		int index = 0;
//	//		for (const Model::Animation& animation : model->GetAnimations())
//	//		{
//	//			ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_Leaf;
//
//	//			ImGui::TreeNodeEx(&animation, nodeFlags, animation.name.c_str());
//
//	//			// ダブルクリックでアニメーション再生
//	//			if (ImGui::IsItemClicked())
//	//			{
//	//				if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
//	//				{
//	//					animationPlaying = true;
//	//					currentAnimationIndex = index;
//	//					currentAnimationSeconds = 0.0f;
//	//					animationSpeed = 1.0f;
//	//				}
//	//			}
//
//	//			ImGui::TreePop();
//
//	//			index++;
//	//		}
//	//	}
//	//}
//
//	//ImGui::End();
//}
//
//// マテリアルGUI描画
//void ModelViewerScene::DrawMaterialGUI()
//{
//	if (ImGui::Begin("Material", nullptr, ImGuiWindowFlags_None))
//	{
//		for (int i = 0; i < debug_models.size(); i++)
//		{
//			Model* debug_model = debug_models[i];
//			if (debug_model)
//			{
//				ImGui::PushID(debug_model);
//
//				if (debug_model != nullptr)
//				{
//					int index = 0;
//					for (const Model::Material& material : debug_model->GetMaterials())
//					{
//						ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow
//							| ImGuiTreeNodeFlags_OpenOnDoubleClick;
//
//						if (ImGui::TreeNodeEx(&material, nodeFlags, material.name.c_str()))
//						{
//							ImGui::Text("BaseMap");
//							ImGui::Image(material.baseMap.Get(), ImVec2(50, 50));
//							Color baseColor = material.baseColor;
//							ImGui::ColorEdit4("BaseColor", &baseColor.x, ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoInputs);
//
//							ImGui::TreePop();
//						}
//
//						index++;
//					}
//				}
//
//				ImGui::Separator();
//
//				ImGui::PopID();
//			}
//		}
//	}
//
//	ImGui::End();
//}
