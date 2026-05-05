#include <functional>
#include <imgui.h>
#include "ModelViewerScene.h"
#include "Graphics.h"
#include "Dialog.h"
#include "GpuResourceUtils.h"

#define DISABLE_MODEL_LOAD

// コンストラクタ
ModelViewerScene::ModelViewerScene()
{
	ID3D11Device* device = Graphics::Instance().GetDevice();
	float screenWidth = Graphics::Instance().GetScreenWidth();
	float screenHeight = Graphics::Instance().GetScreenHeight();

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
	cameraController.SyncCameraToController(camera);

	shaderId = static_cast<int>(ShaderId::Basic);

	// ライト設定
	DirectionalLight directionalLight;
	directionalLight.direction = { 0, -1, -1 };
	directionalLight.color = { 1, 1, 1 };
	lightManager.SetDirectionalLight(directionalLight);

	model_stage = std::make_shared<Model>(device, "Data/Model/Stage/Stage00.glb", animationSamplingRate);
	model_player = std::make_shared<Model>(device, "Data/Model/ToonSoldiers_WW2/models/ToonSoldier_WW2_Japan_Soldier.glb", animationSamplingRate);
	
	// materialにセット
	GpuResourceUtils::LoadTexture(
		Graphics::Instance().GetDevice(),
		"Data/Model/ToonSoldiers_WW2/models/textures/TS_WW2_Japan_Infantry.tga",
		model_player->GetMaterials()[0].baseMap.GetAddressOf()
	);
	GpuResourceUtils::LoadTexture(
		Graphics::Instance().GetDevice(),
		"Data/Model/ToonSoldiers_WW2/models/textures/TS_WW2_weapons.tga",
		model_player->GetMaterials()[1].baseMap.GetAddressOf()
	);

	model_player->GetMeshes()[2].isDraw = false;
	model_player->GetMeshes()[4].isDraw = false;
	model_player->GetMeshes()[5].isDraw = false;
	model_player->GetMeshes()[6].isDraw = false;
	model_player->GetMeshes()[7].isDraw = false;
	model_player->GetMeshes()[8].isDraw = false;
	model_player->GetMeshes()[9].isDraw = false;
	model_player->GetMeshes()[10].isDraw = false; // ガスマスク
	model_player->GetMeshes()[11].isDraw = false; // ガスマスク

	// アニメーション追加
	model_player->AppendAnimations("Data/Model/ToonSoldiers_WW2/animation/Infantry/movement/infantry_combat_walk.glb");

	// アニメーター生成
	animator_player = std::make_shared<Animator>(model_player.get());
	animator_player->Play(0, true);

	// デバッグ用モデルリストに追加
	debug_models.push_back(model_stage.get());
	debug_models.push_back(model_player.get());
}

// 更新処理
void ModelViewerScene::Update(float elapsedTime)
{
	// カメラ更新処理
	cameraController.Update();
	cameraController.SyncControllerToCamera(camera);

#ifndef DISABLE_MODEL_LOAD
	// モデルのあれこれ
	if (model)
	{
		// アニメーション更新
		if (animationPlaying && currentAnimationIndex >= 0)
		{
			model->ComputeAnimation(currentAnimationIndex, currentAnimationSeconds, nodePoses);
			model->SetNodePoses(nodePoses);

			// 時間更新
			const Model::Animation& animation = model->GetAnimations().at(currentAnimationIndex);
			currentAnimationSeconds += elapsedTime * animationSpeed;
			if (currentAnimationSeconds > animation.secondsLength)
			{
				if (animationLoop)
				{
					currentAnimationSeconds -= animation.secondsLength;
				}
				else
				{
					currentAnimationSeconds = animation.secondsLength;
				}
			}
		}
	}
#endif

	if (model_stage)
	{
		// トランスフォーム更新
		animator_player->Update(elapsedTime);
		Matrix worldTransform;
		worldTransform = Matrix::CreateScale(100, 100, 100);
		model_stage->UpdateTransform(worldTransform);
	}

	if (model_player)
	{
		// トランスフォーム更新
		Matrix worldTransform;
		worldTransform = Matrix::CreateScale(1, 1, 1);
		model_player->UpdateTransform(worldTransform);
	}
}

// 描画処理
void ModelViewerScene::Render(float elapsedTime)
{
	ID3D11DeviceContext* dc = Graphics::Instance().GetDeviceContext();
	RenderState* renderState = Graphics::Instance().GetRenderState();
	PrimitiveRenderer* primitiveRenderer = Graphics::Instance().GetPrimitiveRenderer();
	ModelRenderer* modelRenderer = Graphics::Instance().GetModelRenderer();

	// グリッド描画
#ifdef _DEBUG
	if (GetKeyState(VK_CONTROL) & 0x8000) {
		primitiveRenderer->DrawGrid(100, 1);
		primitiveRenderer->Render(dc, camera.GetView(), camera.GetProjection(), D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	}
#endif

	// 描画コンテキスト設定
	RenderContext rc;
	rc.deviceContext = dc;
	rc.renderState = renderState;
	rc.camera = &camera;
	rc.lightManager = &lightManager;

	if (model_stage != nullptr)
	{
		// モデル描画
		modelRenderer->Draw(ShaderId::Lambert, model_stage);
		modelRenderer->Render(rc);
	}

	if (model_player != nullptr)
	{
		// モデル描画
		modelRenderer->Draw(ShaderId::Lambert, model_player);
		modelRenderer->Render(rc);
	}
}

// GUI描画処理
void ModelViewerScene::DrawGUI()
{
	DrawHierarchyGUI();
	DrawPropertyGUI();
	DrawAnimationGUI();
	DrawMaterialGUI();
}

// ヒエラルキーGUI描画
void ModelViewerScene::DrawHierarchyGUI()
{
	if (ImGui::Begin("Hierarchy", nullptr, ImGuiWindowFlags_None))
	{
		for (int i = 0; i < debug_models.size(); i++)
		{
			Model* debug_model = debug_models[i];
			if (debug_model)
			{
				ImGui::PushID(debug_model);

				// ノードツリーを再帰的に描画する関数
				std::function<void(Model::Node*)> drawNodeTree = [&](Model::Node* node)
					{
						// 矢印をクリック、またはノードをダブルクリックで階層を開く
						ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow
							| ImGuiTreeNodeFlags_OpenOnDoubleClick;

						// 子がいない場合は矢印をつけない
						size_t childCount = node->children.size();
						if (childCount == 0)
						{
							nodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
						}

						// 選択フラグ
						if (selectionNode == node)
						{
							nodeFlags |= ImGuiTreeNodeFlags_Selected;
						}

						bool isAnyMeshHidden = false;
						std::string meshIndices = "";

						// このノードに関連するメッシュを探す
						for (int i = 0; i < debug_model->GetMeshes().size(); i++)
						{
							const Model::Mesh& mesh = debug_model->GetMeshes()[i];
							if (mesh.node == node)
							{
								if (!meshIndices.empty()) meshIndices += ",";
								meshIndices += std::to_string(i);

								if (!mesh.isDraw)
									isAnyMeshHidden = true;
							}
						}

						// ツリーノードを表示
						ImGui::PushStyleColor(ImGuiCol_Text,
							IM_COL32(255, 255, 255, isAnyMeshHidden ? 100 : 255));

						int nodeIndex = static_cast<int>(node - debug_model->GetNodes().data());
						bool opened = ImGui::TreeNodeEx(node, nodeFlags,
							("[" + std::to_string(nodeIndex) + "]"
								+ (meshIndices.empty() ? "" : "{" + meshIndices + "}")
								+ node->name).c_str());

						ImGui::PopStyleColor();

						// フォーカスされたノードを選択する
						if (ImGui::IsItemFocused())
						{
							selectionNode = node;
						}

						// 開かれている場合、子階層も同じ処理を行う
						if (opened && childCount > 0)
						{
							for (Model::Node* child : node->children)
							{
								drawNodeTree(child);
							}
							ImGui::TreePop();
						}
					};

				// 再帰的にノードを描画
				drawNodeTree(debug_model->GetRootNode());

				ImGui::PopID();
			}
		}
	}
	ImGui::End();
}

// プロパティGUI描画
void ModelViewerScene::DrawPropertyGUI()
{
	if (ImGui::Begin("Property", nullptr, ImGuiWindowFlags_None))
	{
		if (selectionNode != nullptr)
		{
			if (ImGui::CollapsingHeader("Node", ImGuiTreeNodeFlags_DefaultOpen))
			{
				// 位置
				if (ImGui::DragFloat3("Local Position", &selectionNode->position.x, 0.1f))
				{
					animationPlaying = false;
				}

				Vector3 globalPos, globalSca;
				Quaternion globalRot;
				selectionNode->globalTransform.Decompose(globalPos, globalRot, globalSca);
				Vector3 globalRotEuler = globalRot.ToEuler();

				// 位置
				if (ImGui::DragFloat3("Global Position", &globalPos.x, 0.1f))
				{
					if (selectionNode->parent != nullptr)
					{
						DirectX::XMMATRIX ParentGlobalTransform = selectionNode->parent->globalTransform;
						DirectX::XMMATRIX InverseParentGlobalTransform = DirectX::XMMatrixInverse(nullptr, ParentGlobalTransform);
						DirectX::XMVECTOR LocalPosition = Vector3::Transform(globalPos, InverseParentGlobalTransform);
						selectionNode->position = LocalPosition;
					}
					else
					{
						selectionNode->position = globalPos;
					}
					animationPlaying = false;
				}

				// 回転
				Vector3 angle = selectionNode->rotation.ToEuler();
				if (ImGui::DragFloat3("Local Rotation", &angle.x, 1.0f))
				{
					selectionNode->rotation = Quaternion::CreateFromYawPitchRoll(angle.y, angle.x, angle.z);
				}

				if (ImGui::DragFloat3("Global Rotation", &globalRotEuler.x, 0.1f))
				{
					Quaternion GlobalRotation = Quaternion::CreateFromYawPitchRoll(globalRotEuler.y, globalRotEuler.x, globalRotEuler.z);

					if (selectionNode->parent != nullptr)
					{
						DirectX::XMMATRIX ParentGlobalTransform = selectionNode->parent->globalTransform;
						ParentGlobalTransform.r[0] = DirectX::XMVector3Normalize(ParentGlobalTransform.r[0]);
						ParentGlobalTransform.r[1] = DirectX::XMVector3Normalize(ParentGlobalTransform.r[1]);
						ParentGlobalTransform.r[2] = DirectX::XMVector3Normalize(ParentGlobalTransform.r[2]);
						DirectX::XMVECTOR ParentGlobalRotation = DirectX::XMQuaternionRotationMatrix(ParentGlobalTransform);
						DirectX::XMVECTOR InverseParentGlobalRotation = DirectX::XMQuaternionInverse(ParentGlobalRotation);
						DirectX::XMVECTOR LocalRotation = DirectX::XMQuaternionMultiply(GlobalRotation, InverseParentGlobalRotation);
						selectionNode->rotation = LocalRotation;
					}
					else
					{
						selectionNode->rotation = GlobalRotation;
					}
					animationPlaying = false;
				}

				// スケール
				if (ImGui::DragFloat3("Local Scale", &selectionNode->scale.x, 0.01f))
				{
					animationPlaying = false;
				}

				if (ImGui::DragFloat3("Global Scale", &globalSca.x, 0.1f))
				{
				}

			}
		}
	}

	ImGui::End();
}

// アニメーションGUI描画
void ModelViewerScene::DrawAnimationGUI()
{
	if (ImGui::Begin("Animation", nullptr, ImGuiWindowFlags_None))
	{
		ImGui::Checkbox("Loop", &animationLoop); ImGui::SameLine();
		ImGui::SetNextItemWidth(70);
		ImGui::InputFloat("SamplingRate", &animationSamplingRate);
		ImGui::DragFloat("BlendSeconds", &animationBlendSeconds, 0.01f);

		if (model != nullptr)
		{
			float secondsLength = currentAnimationIndex >= 0 ? model->GetAnimations().at(currentAnimationIndex).secondsLength : 0;
			int currentFrame = static_cast<int>(currentAnimationSeconds * 60.0f);
			int frameLength = static_cast<int>(secondsLength * 60);

			ImGui::SetNextItemWidth(50);
			ImGui::PushID(u8"フレーム");
			if (ImGui::DragInt("##v", &currentFrame, 1, 0, frameLength))
			{
				animationPlaying = true;
				currentAnimationSeconds = currentFrame / 60.0f;
				animationSpeed = 0.0f;
			}
			ImGui::PopID();

			ImGui::SameLine();
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
			ImGui::PushID(u8"タイムライン");
			if (ImGui::SliderFloat("##v", &currentAnimationSeconds, 0, secondsLength, "%.3f"))
			{
				animationPlaying = true;
				animationSpeed = 0.0f;
			}
			ImGui::PopID();

			int index = 0;
			for (const Model::Animation& animation : model->GetAnimations())
			{
				ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_Leaf;

				ImGui::TreeNodeEx(&animation, nodeFlags, animation.name.c_str());

				// ダブルクリックでアニメーション再生
				if (ImGui::IsItemClicked())
				{
					if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
					{
						animationPlaying = true;
						currentAnimationIndex = index;
						currentAnimationSeconds = 0.0f;
						animationSpeed = 1.0f;
					}
				}

				ImGui::TreePop();

				index++;
			}
		}
	}

	ImGui::End();
}

// マテリアルGUI描画
void ModelViewerScene::DrawMaterialGUI()
{
	if (ImGui::Begin("Material", nullptr, ImGuiWindowFlags_None))
	{
		if (model != nullptr)
		{
			int index = 0;
			for (const Model::Material& material : model->GetMaterials())
			{
				ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow
					| ImGuiTreeNodeFlags_OpenOnDoubleClick;

				if (ImGui::TreeNodeEx(&material, nodeFlags, material.name.c_str()))
				{
					ImGui::Text("BaseMap");
					ImGui::Image(material.baseMap.Get(), ImVec2(50, 50));
					DirectX::XMFLOAT4 baseColor = material.baseColor;
					ImGui::ColorEdit4("BaseColor", &baseColor.x, ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoInputs);

					ImGui::TreePop();
				}

				index++;
			}
		}
	}

	ImGui::End();
}
