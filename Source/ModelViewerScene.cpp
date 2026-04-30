#include <functional>
#include <imgui.h>
#include "ModelViewerScene.h"
#include "Graphics.h"
#include "Dialog.h"

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
}

// 更新処理
void ModelViewerScene::Update(float elapsedTime)
{
	// カメラ更新処理
	cameraController.Update();
	cameraController.SyncControllerToCamera(camera);

	// モデルのあれこれ
	if (model != nullptr)
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

		// トランスフォーム更新
		Matrix worldTransform;
		worldTransform = Matrix::Identity;
		model->UpdateTransform(worldTransform);
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
	primitiveRenderer->DrawGrid(20, 1);
	primitiveRenderer->Render(dc, camera.GetView(), camera.GetProjection(), D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

	// 描画コンテキスト設定
	RenderContext rc;
	rc.deviceContext = dc;
	rc.renderState = renderState;
	rc.camera = &camera;
	rc.lightManager = &lightManager;

	// 描画
	if (model != nullptr)
	{
		// モデル描画
		modelRenderer->Draw(static_cast<ShaderId>(shaderId), model);
		modelRenderer->Render(rc);

		// レンダーステート設定
		dc->OMSetBlendState(renderState->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
		dc->OMSetDepthStencilState(renderState->GetDepthStencilState(DepthState::NoTestNoWrite), 0);
		dc->RSSetState(renderState->GetRasterizerState(RasterizerState::SolidCullNone));

		// 軸描画
		const std::vector<Model::Node>& nodes = model->GetNodes();
		if (selectionNode != nullptr)
		{
			Vector3 p, x, y, z;
			const float length = 0.1f;
			DirectX::XMMATRIX W = selectionNode->worldTransform;
			DirectX::XMVECTOR X = Vector3::Transform(Vector3(length, 0, 0), W);
			DirectX::XMVECTOR Y = Vector3::Transform(Vector3(0, length, 0), W);
			DirectX::XMVECTOR Z = Vector3::Transform(Vector3(0, 0, length), W);
			p = W.r[3];
			x = X;
			y = Y;
			z = Z;
			primitiveRenderer->AddVertex(p, { 1, 0, 0, 1 });
			primitiveRenderer->AddVertex(x, { 1, 0, 0, 1 });
			primitiveRenderer->AddVertex(p, { 0, 1, 0, 1 });
			primitiveRenderer->AddVertex(y, { 0, 1, 0, 1 });
			primitiveRenderer->AddVertex(p, { 0, 0, 1, 1 });
			primitiveRenderer->AddVertex(z, { 0, 0, 1, 1 });
		}
		primitiveRenderer->Render(dc, camera.GetView(), camera.GetProjection(), D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	}
}

// GUI描画処理
void ModelViewerScene::DrawGUI()
{
	DrawMenuGUI();
	DrawHierarchyGUI();
	DrawPropertyGUI();
	DrawAnimationGUI();
	DrawMaterialGUI();
}

// メニューGUI描画
void ModelViewerScene::DrawMenuGUI()
{
	if (ImGui::BeginMainMenuBar())
	{
		// ファイルメニュー
		if (ImGui::BeginMenu("File"))
		{
			bool check = false;
			if (ImGui::MenuItem("Open Model", "", &check))
			{
				static const char* filter = "Model Files(*.gltf;*.glb)\0*.gltf;*.glb;\0All Files(*.*)\0*.*;\0\0";

				char filename[256] = { 0 };
				HWND hWnd = Graphics::Instance().GetWindowHandle();
				DialogResult result = Dialog::OpenFileName(filename, sizeof(filename), filter, nullptr, hWnd);
				if (result == DialogResult::OK)
				{
					ID3D11Device* device = Graphics::Instance().GetDevice();
					model = std::make_shared<Model>(device, filename, animationSamplingRate);
					animationSpeed = 1.0f;
					currentAnimationSeconds = 0.0f;
					currentAnimationIndex = -1;
				}
			}

			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}
}

// ヒエラルキーGUI描画
void ModelViewerScene::DrawHierarchyGUI()
{
	if (ImGui::Begin("Hierarchy", nullptr, ImGuiWindowFlags_None))
	{
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

			// ツリーノードを表示
			bool opened = ImGui::TreeNodeEx(node, nodeFlags, node->name.c_str());

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
		if (model != nullptr)
		{
			drawNodeTree(model->GetRootNode());
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
			const char* shaderNames[] =
			{
				"Basic",
				"Lambert",
			};
			ImGui::Combo("Shader", &shaderId, shaderNames, _countof(shaderNames));

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
