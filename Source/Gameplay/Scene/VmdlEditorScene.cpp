// VmdlEditorScene.cpp

#include "Gameplay/Scene/VmdlEditorScene.h"

#include "Application/SettingsAndDebug/PhysicsLayerManager.h"
#include "Application/Tools/Dialog.h"
#include "Core/Object/Object.h"
#include "Gameplay/Camera/Camera.h"
#include "Rendering/Core/Graphics.h"
#include "Resource/VMDLModel.h"

#include "SceneManager.h"

#include <imgui.h>
#include <imguizmo/ImGuizmo.h>

#include <algorithm>
#include <cfloat>
#include <cctype>
#include <cmath>
#include "GameStartScene.h"
#include "Application/Time/GameTime.h"
#include "Resource/ResourceManager.h"

constexpr UINT PreviewWidth = 1024;
constexpr UINT PreviewHeight = 1024;
constexpr int PreviewGridSubdivisions = 20;
constexpr float PreviewGridScale = 0.5f;
constexpr float PreviewMinCameraDistance = 0.2f;
constexpr float PreviewMaxCameraDistance = 100000.0f;

std::string ToUpperMorphName(std::string value)
{
	std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
	{
		return static_cast<char>(std::toupper(c));
	});
	return value;
}

std::string MakeUniqueMorphName(const VMDLModel& model, const std::string& baseName)
{
	const std::string base = ToUpperMorphName(baseName);
	if (model.GetMorphIndex(base.c_str()) < 0) return base;
	for (int suffix = 2; ; ++suffix)
	{
		const std::string candidate = base + " " + std::to_string(suffix);
		if (model.GetMorphIndex(candidate.c_str()) < 0) return candidate;
	}
}

std::string MakeNodeLabel(int nodeIndex, const std::string& nodeName)
{
	return std::to_string(nodeIndex) + ":" + nodeName;
}

Vector3 EvaluateVectorKeys(const std::vector<VMDLModel::VectorKeyframe>& keys, float time, const Vector3& fallback)
{
	if (keys.empty()) return fallback;
	if (keys.size() == 1 || time <= keys.front().seconds) return keys.front().value;
	if (time >= keys.back().seconds) return keys.back().value;
	for (size_t i = 1; i < keys.size(); ++i)
	{
		if (time > keys[i].seconds) continue;
		const float duration = keys[i].seconds - keys[i - 1].seconds;
		const float rate = duration > 0.00001f ? (time - keys[i - 1].seconds) / duration : 0.0f;
		return Vector3::Lerp(keys[i - 1].value, keys[i].value, rate);
	}
	return keys.back().value;
}

Quaternion EvaluateQuaternionKeys(const std::vector<VMDLModel::QuaternionKeyframe>& keys, float time, const Quaternion& fallback)
{
	if (keys.empty()) return fallback;
	if (keys.size() == 1 || time <= keys.front().seconds) return keys.front().value;
	if (time >= keys.back().seconds) return keys.back().value;
	for (size_t i = 1; i < keys.size(); ++i)
	{
		if (time > keys[i].seconds) continue;
		const float duration = keys[i].seconds - keys[i - 1].seconds;
		const float rate = duration > 0.00001f ? (time - keys[i - 1].seconds) / duration : 0.0f;
		return Quaternion::Slerp(keys[i - 1].value, keys[i].value, rate);
	}
	return keys.back().value;
}

VmdlEditorScene::VmdlEditorScene(SceneMessage message)
	: Scene(message)
{
	Game::Graphics& graphics = Game::Graphics::Instance();

	HWND window = graphics.GetWindowHandle();

	previousWindowStyle =
		GetWindowLongPtr(
		window,
		GWL_STYLE);

	previousWindowPlacement.length =
		sizeof(previousWindowPlacement);

	restoreWindowOnExit =
		GetWindowPlacement(
		window,
		&previousWindowPlacement) != FALSE;

	graphics.SetBorderlessFullscreen(true);
	graphics.SetWindowMovementLocked(true);

	previewSceneTarget =
		std::make_unique<RenderTarget>(
		graphics.GetDevice(),
		PreviewWidth,
		PreviewHeight,
		DXGI_FORMAT_R16G16B16A16_FLOAT);

	previewTarget =
		std::make_unique<RenderTarget>(
		graphics.GetDevice(),
		PreviewWidth,
		PreviewHeight,
		DXGI_FORMAT_R8G8B8A8_UNORM);

	cameraOwner =
		std::make_unique<Object>(
		"VMDL Editor Camera");

	editorCamera =
		cameraOwner->AddComponent<Camera>();

	editorLightDirection =
	{0.6f, -0.7f, 0.0f};
}

VmdlEditorScene::~VmdlEditorScene()
{
	Game::Graphics& graphics = Game::Graphics::Instance();
	graphics.SetBorderlessFullscreen(false);
	graphics.SetWindowMovementLocked(false);
}

void VmdlEditorScene::OnDrawGUI()
{
	UpdateWindowTitle();
	if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O, false))
	{
		OpenVmdl();
	}
	else if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
	{
		if (ImGui::GetIO().KeyShift) SaveVmdlAs();
		else SaveVmdl();
	}
	if (animationPlaying && model && selectedAnimation >= 0 && selectedAnimation < static_cast<int>(model->GetAnimations().size()))
	{
		const float length = model->GetAnimations()[selectedAnimation].secondsLength;
		animationTime += ImGui::GetIO().DeltaTime * playbackSpeed;
		if (animationTime > length)
		{
			if (animationLoop && length > 0.0f) animationTime = std::fmod(animationTime, length);
			else
			{
				animationTime = length;
				animationPlaying = false;
			}
		}
		ApplyAnimationPreview();
	}
	RenderPreview();
	ImGuizmo::BeginFrame();

	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);

	constexpr ImGuiWindowFlags flags =
		ImGuiWindowFlags_MenuBar |
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoSavedSettings;

	if (!ImGui::Begin("VMDL Editor", nullptr, flags))
	{
		ImGui::End();
		return;
	}

	DrawMenuBar();
	const float totalHeight = ImGui::GetContentRegionAvail().y;
	bottomPanelHeight = std::clamp(bottomPanelHeight, 140.0f, std::max(140.0f, totalHeight - 220.0f));
	const float splitterHeight = 6.0f;
	const float upperHeight = std::max(220.0f, totalHeight - bottomPanelHeight - splitterHeight);
	const float totalWidth = ImGui::GetContentRegionAvail().x;
	const float previewAspect = static_cast<float>(PreviewWidth) / static_cast<float>(PreviewHeight);
	const float viewportWidth = std::min(upperHeight * previewAspect, std::max(220.0f, totalWidth - 440.0f));
	constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp;
	if (ImGui::BeginTable("Main Editor Layout", 3, tableFlags, ImVec2(0.0f, upperHeight)))
	{
		ImGui::TableSetupColumn("Property Column", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableSetupColumn("Viewport Column", ImGuiTableColumnFlags_WidthFixed, viewportWidth);
		ImGui::TableSetupColumn("Hierarchy Column", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::BeginChild("Property", ImVec2(0.0f, upperHeight), true);
		DrawProperty();
		ImGui::EndChild();
		ImGui::TableSetColumnIndex(1);
		ImGui::BeginChild("3D View", ImVec2(0.0f, upperHeight), true, ImGuiWindowFlags_NoScrollbar);
		DrawViewport();
		ImGui::EndChild();
		ImGui::TableSetColumnIndex(2);
		ImGui::BeginChild("Hierarchy", ImVec2(0.0f, upperHeight), true);
		DrawHierarchy();
		ImGui::EndChild();
		ImGui::EndTable();
	}
	ImGui::Button("##BottomSplitter", ImVec2(-1.0f, splitterHeight));
	if (ImGui::IsItemActive()) bottomPanelHeight = std::clamp(bottomPanelHeight - ImGui::GetIO().MouseDelta.y, 140.0f, totalHeight - 220.0f);
	if (ImGui::IsItemHovered() || ImGui::IsItemActive()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);

	ImGui::BeginChild("Editor Bottom", ImVec2(0.0f, 0.0f), true);
	if (ImGui::BeginTabBar("VMDL Editor Tabs"))
	{
		if (ImGui::BeginTabItem("Animation"))
		{
			DrawTimeline();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Morph"))
		{
			DrawMorphEditor();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("IK Settings"))
		{
			DrawIkSettings();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Material"))
		{
			DrawMaterialEditor();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
	ImGui::EndChild();

	if (animationRecording)
	{
		const ImVec2 position = ImGui::GetWindowPos();
		const ImVec2 size = ImGui::GetWindowSize();

		constexpr float thickness = 4.0f;
		constexpr float inset = thickness * 0.5f;

		ImGui::GetForegroundDrawList()->AddRect(
			ImVec2(position.x + inset, position.y + inset),
			ImVec2(position.x + size.x - inset, position.y + size.y - inset),
			IM_COL32(255, 0, 0, 255),
			0.0f,
			ImDrawFlags_None,
			thickness);
	}

	ImGui::End();
	if (showPhysicsLayerWindow) PhysicsLayerManager::Instance().DrawGUI(&showPhysicsLayerWindow);

	if (showSetScaleWindow) ImGui::OpenPopup("Set Scale");
	showSetScaleWindow = false;
	if (ImGui::BeginPopupModal("Set Scale", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::SetNextItemWidth(220.0f);
		ImGui::InputFloat("Scale", &setScaleValue, 0.01f, 0.1f, "%.4f");
		const bool validScale = model && std::isfinite(setScaleValue) && setScaleValue > 0.0f;
		ImGui::BeginDisabled(!validScale);
		if (ImGui::Button("Apply", ImVec2(100.0f, 0.0f)))
		{
			model->SetModelScale(setScaleValue);
			setScaleValue = model->GetModelScale();
			UpdateModelFraming();
			MarkDirty();
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(100.0f, 0.0f))) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
}

void VmdlEditorScene::RenderPreview()
{
	if (!previewSceneTarget || !previewTarget || !editorCamera) return;

	const float zoomLerp = 1.0f - std::exp(-12.0f * Game::Time::unscaledDeltaTime);
	cameraDistance = std::lerp(cameraDistance, targetCameraDistance, zoomLerp);
	const Vector3 focus = Vector3(0.0f, 1.0f, 0.0f) + cameraFocusOffset;
	const float horizontalDistance = cameraDistance * std::cos(cameraPitch);
	const Vector3 eye = focus + Vector3(
		std::sin(cameraYaw) * horizontalDistance,
		std::sin(cameraPitch) * cameraDistance,
		-std::cos(cameraYaw) * horizontalDistance);
	editorCamera->SetLookAt(eye, focus, Vector3::Up);
	const float nearClip = std::max(0.01f, cameraDistance * 0.0001f);
	const float farClip = std::max(1000.0f, cameraDistance * 2.0f);
	editorCamera->SetPerspectiveFov(DirectX::XMConvertToRadians(45.0f), 1.0f, nearClip, farClip);

	Game::Graphics& graphics = Game::Graphics::Instance();
	ID3D11DeviceContext* dc = graphics.GetDeviceContext();
	previewSceneTarget->Clear(dc, 0, 0, 0, 1.0f);
	previewSceneTarget->Activate(dc);

	RenderContext rc{};
	rc.deviceContext = dc;
	rc.renderState = graphics.GetRenderState();
	rc.camera = editorCamera;
	editorLights.GetDirectionalLight().transform.SetAngle(editorLightDirection);
	editorLightDirection.y += Game::Time::unscaledDeltaTime * 50.0f;
	if (editorLightDirection.y > 360.0f) editorLightDirection.y -= 360.0f;
	rc.lightManager = &editorLights;
	rc.iblData.diffuseIrradianceEnvironmentMap = graphics.GetIBLDiffuseIEM();
	rc.iblData.specularPremappingRadianceEnvironmentMap = graphics.GetIBLSpecularPMREM();
	rc.iblData.ggxLookUpTableMap = graphics.GetIBLGGXLUT();

	if (showGrid)
	{
		graphics.GetPrimitiveRenderer()->DrawGrid(PreviewGridSubdivisions, PreviewGridScale);
		graphics.GetPrimitiveRenderer()->Render(dc, editorCamera->GetView(), editorCamera->GetProjection(), D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	}

	if (model)
	{
		model->UpdateTransform(Matrix::Identity);
		if (showMesh)
		{
			rc.renderSettings.wireframe = false;
			VMatRenderParams params;
			if (!usePbr)
			{
				for (const VMDLModel::Material& material : model->GetMaterials())
				{
					params.materials[material.name] = {
						.baseColor = solidColor,
						.useBaseColorTexture = false,
					};
				}
			}
			graphics.GetModelRenderer()->Draw(ModelShaderId::VMat, model, &params);
			graphics.GetModelRenderer()->Render(rc);
		}
		if (showFaces)
		{
			rc.renderSettings.wireframe = true;
			graphics.GetModelRenderer()->Draw(ModelShaderId::VMat, model);
			graphics.GetModelRenderer()->Render(rc);
		}

		if (showBones)
		{
			const auto& nodes = model->GetNodes();
			const Matrix renderScaleTransform = model->GetRenderScaleTransform();
			for (const VMDLModel::Node& node : nodes)
			{
				if (!node.parent) continue;
				const Vector3 start = (node.parent->worldTransform * renderScaleTransform).Translation();
				const Vector3 end = (node.worldTransform * renderScaleTransform).Translation();
				graphics.GetPrimitiveRenderer()->DrawLine(start, end, Color(1, 0.8f, 0.1f, 1), Color(1, 0.3f, 0.1f, 1));
			}
			graphics.GetPrimitiveRenderer()->Render(dc, editorCamera->GetView(), editorCamera->GetProjection(), D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		}

		const auto nodeOffsetTransform = [&](int nodeIndex, const Vector3& offset, const Vector3& rotation = Vector3::Zero)
		{
			const Matrix local =
				Matrix::CreateFromYawPitchRoll(RAD(rotation.y), RAD(rotation.x), RAD(rotation.z)) *
				Matrix::CreateTranslation(offset);
			if (nodeIndex < 0 || nodeIndex >= static_cast<int>(model->GetNodes().size())) return local;
			return local * model->GetNodes()[nodeIndex].worldTransform;
		};
		const auto matrixPosition = [](const Matrix& transform)
		{
			return Vector3(transform._41, transform._42, transform._43);
		};
		auto& data = model->GetVmdlExtensionData();
		bool hasShapes = false;
		if (showRigidBody)
		{
			for (const auto& value : data.rigidBodies)
			{
				Matrix transform = nodeOffsetTransform(value.nodeIndex, value.offsetPosition, value.offsetRotation);
				Vector3 scale;
				Vector3 position;
				Quaternion rotation;
				transform.Decompose(scale, rotation, position);
				graphics.GetShapeRenderer()->DrawBox(position, rotation.ToEuler(), Vector3(0.2f, 0.2f, 0.2f), Color(1.0f, 0.75f, 0.1f, 0.7f));
				hasShapes = true;
			}
		}
		if (showCollider)
		{
			for (int i = 0; i < static_cast<int>(data.colliders.size()); ++i)
			{
				if (i < static_cast<int>(previewColliderActive.size()) && previewColliderActive[i] == 0) continue;
				const auto& value = data.colliders[i];
				Matrix transform = model->GetScaledAttachmentTransform(
					nodeOffsetTransform(value.nodeIndex, value.center, value.rotation));
				const Vector3 scaledSize = model->GetScaledAttachmentVector(value.size);
				Vector3 transformScale;
				Vector3 position;
				Quaternion rotation;
				transform.Decompose(transformScale, rotation, position);
				const Matrix pose = Matrix::CreateFromQuaternion(rotation) * Matrix::CreateTranslation(position);
				if (value.shape == 1) graphics.GetShapeRenderer()->DrawSphere(position, std::max(0.001f, scaledSize.x), Color(0.1f, 0.9f, 1.0f, 0.7f));
				else if (value.shape == 2) graphics.GetShapeRenderer()->DrawCapsule(pose, std::max(0.001f, scaledSize.x), std::max(0.001f, scaledSize.y), Color(0.1f, 0.9f, 1.0f, 0.7f));
				else
				{
					graphics.GetShapeRenderer()->DrawBox(position, rotation.ToEuler(), scaledSize, Color(0.1f, 0.9f, 1.0f, 0.7f));
				}
				hasShapes = true;
			}
		}
		if (showSpringCollider)
		{
			for (const auto& value : data.springColliders)
			{
				graphics.GetShapeRenderer()->DrawSphere(
					matrixPosition(
					nodeOffsetTransform(
					value.nodeIndex, value.offsetPosition)), value.radius, Color(1.0f, 0.2f, 0.9f, 0.7f));
				hasShapes = true;
			}
		}
		if (hasShapes) graphics.GetShapeRenderer()->Render(dc, editorCamera->GetView(), editorCamera->GetProjection());
		if (showSpring)
		{
			for (const auto& value : data.springs)
			{
				const Vector3 start = matrixPosition(nodeOffsetTransform(value.nodeIndex, Vector3::Zero));
				const Vector3 end = matrixPosition(nodeOffsetTransform(value.nodeIndex, value.offsetPosition));
				graphics.GetPrimitiveRenderer()->DrawLine(start, end, Color(0.3f, 1.0f, 0.3f, 1.0f), Color(0.1f, 0.5f, 0.1f, 1.0f));
			}
			graphics.GetPrimitiveRenderer()->Render(dc, editorCamera->GetView(), editorCamera->GetProjection(), D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		}
		if (showTrail)
		{
			const auto& trails = model->GetVmdlTrailData().trails;
			for (int i = 0; i < static_cast<int>(trails.size()); ++i)
			{
				if (i < static_cast<int>(previewTrailActive.size()) && previewTrailActive[i] == 0) continue;
				const auto& value = trails[i];
				const Vector3 root = matrixPosition(nodeOffsetTransform(value.nodeIndex, value.rootOffset));
				const Vector3 tip = matrixPosition(nodeOffsetTransform(value.nodeIndex, value.tipOffset));
				graphics.GetPrimitiveRenderer()->DrawLine(root, tip, value.color, value.color);
			}
			graphics.GetPrimitiveRenderer()->Render(dc, editorCamera->GetView(), editorCamera->GetProjection(), D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		}
	}

	previewSceneTarget->Deactivate(dc);

	previewTarget->Clear(dc);
	previewTarget->Activate(dc);
	postProcess.ToneMapping(rc, previewSceneTarget->GetSRV());
	previewTarget->Deactivate(dc);
}

void VmdlEditorScene::DrawMenuBar()
{
	if (!ImGui::BeginMenuBar()) return;
	if (ImGui::BeginMenu("File"))
	{
		if (ImGui::MenuItem("Open VMDL...", "Ctrl+O")) OpenVmdl();
		if (ImGui::MenuItem("Save VMDL", "Ctrl+S", false, model != nullptr)) SaveVmdl();
		if (ImGui::MenuItem("Save VMDL As...", "Ctrl+Shift+S", false, model != nullptr)) SaveVmdlAs();
		ImGui::Separator();
		if (ImGui::MenuItem("Import GLB...")) ImportGlb();
		ImGui::Separator();
		if (ImGui::MenuItem("Exit")) exiting = true;
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Display"))
	{
		if (ImGui::MenuItem("Solid", nullptr, !usePbr)) usePbr = false;
		if (ImGui::MenuItem("PBR", nullptr, usePbr)) usePbr = true;
		ImGui::Separator();
		ImGui::MenuItem("Mesh", nullptr, &showMesh);
		ImGui::MenuItem("Face", nullptr, &showFaces);
		ImGui::MenuItem("Bone", nullptr, &showBones);
		ImGui::MenuItem("Rigid body", nullptr, &showRigidBody);
		ImGui::MenuItem("Collider", nullptr, &showCollider);
		ImGui::MenuItem("Spring", nullptr, &showSpring);
		ImGui::MenuItem("Spring Collider", nullptr, &showSpringCollider);
		ImGui::MenuItem("Trail", nullptr, &showTrail);
		ImGui::MenuItem("Grid", nullptr, &showGrid);
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Window"))
	{
		if (ImGui::MenuItem("Physics Layer")) showPhysicsLayerWindow = true;
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Tools"))
	{
		if (ImGui::MenuItem("Set Scale...", nullptr, false, model != nullptr))
		{
			setScaleValue = model->GetModelScale();
			showSetScaleWindow = true;
		}
		ImGui::EndMenu();
	}
	std::string title = documentPath.empty() ? "Untitled" : documentPath.string();

	if (dirty)
	{
		title += " *";
	}

	const float titleWidth = ImGui::CalcTextSize(title.c_str()).x;
	const float rightPadding = 12.0f;

	ImGui::SetCursorPosX(
		ImGui::GetWindowWidth()
		- titleWidth
		- rightPadding);

	ImGui::TextUnformatted(title.c_str());
	ImGui::EndMenuBar();

	if (exiting)
		SceneManager::Instance().LoadScene<GameStartScene>();
}

void VmdlEditorScene::DrawHierarchy()
{
	ImGui::TextUnformatted("Hierarchy");
	ImGui::Separator();
	if (!model)
	{
		ImGui::TextDisabled("No model loaded.");
		return;
	}

	const auto& nodes = model->GetNodes();
	for (int index = 0; index < static_cast<int>(nodes.size()); ++index)
	{
		if (nodes[index].parentIndex < 0) DrawNodeTree(index);
	}
}

void VmdlEditorScene::DrawNodeTree(int nodeIndex)
{
	const auto& nodes = model->GetNodes();
	const VMDLModel::Node& node = nodes[nodeIndex];
	bool hasMesh = false;
	for (const VMDLModel::Mesh& mesh : model->GetMeshes())
	{
		if (mesh.nodeIndex == nodeIndex) hasMesh = true;
	}

	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
	if (node.children.empty() && !hasMesh) flags |= ImGuiTreeNodeFlags_Leaf;
	if (selectedNode == nodeIndex && selectedMesh < 0) flags |= ImGuiTreeNodeFlags_Selected;
	const std::string nodeLabel = MakeNodeLabel(nodeIndex, node.name);
	const bool open = ImGui::TreeNodeEx(
		reinterpret_cast<void*>(static_cast<intptr_t>(nodeIndex + 1)),
		flags,
		"%s",
		nodeLabel.c_str());
	const bool nodeClicked = ImGui::IsItemClicked();
	DrawNodeContextMenu(nodeIndex);
	const auto& extension = model->GetVmdlExtensionData();
	const auto drawBadge = [](const char* text)
	{
		ImGui::SameLine(0.0f, 4.0f);
		ImGui::TextUnformatted(text);
	};
	if (std::any_of(extension.rigidBodies.begin(), extension.rigidBodies.end(), [nodeIndex](const auto& value) { return value.nodeIndex == nodeIndex; }))
		drawBadge("[RB]");
	if (std::any_of(extension.colliders.begin(), extension.colliders.end(), [nodeIndex](const auto& value) { return value.nodeIndex == nodeIndex; }))
		drawBadge("[C]");
	if (std::any_of(extension.springs.begin(), extension.springs.end(), [nodeIndex](const auto& value) { return value.nodeIndex == nodeIndex; }))
		drawBadge("[S]");
	if (std::any_of(extension.springColliders.begin(), extension.springColliders.end(), [nodeIndex](const auto& value) { return value.nodeIndex == nodeIndex; }))
		drawBadge("[SC]");
	const auto& trails = model->GetVmdlTrailData().trails;
	if (std::any_of(trails.begin(), trails.end(), [nodeIndex](const auto& value) { return value.nodeIndex == nodeIndex; }))
		drawBadge("[T]");
	if (nodeClicked)
	{
		selectedNode = nodeIndex;
		selectedMesh = -1;
		selectedKeyTrack = -1;
		selectedKeyIndex = -1;
	}
	if (!open) return;

	const auto& meshes = model->GetMeshes();
	for (int meshIndex = 0; meshIndex < static_cast<int>(meshes.size()); ++meshIndex)
	{
		const VMDLModel::Mesh& mesh = meshes[meshIndex];
		if (mesh.nodeIndex != nodeIndex) continue;
		const std::string label = "Mesh " + std::to_string(meshIndex) + " : " + mesh.material->name;
		if (!mesh.isDraw) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		if (ImGui::Selectable(label.c_str(), selectedMesh == meshIndex))
		{
			selectedMesh = meshIndex;
			selectedNode = nodeIndex;
			selectedMaterial = mesh.materialIndex;
		}
		if (!mesh.isDraw) ImGui::PopStyleColor();
	}
	for (const VMDLModel::Node* child : node.children)
	{
		DrawNodeTree(static_cast<int>(child - nodes.data()));
	}
	ImGui::TreePop();
}

void VmdlEditorScene::DrawNodeContextMenu(int nodeIndex)
{
	ImGui::PushID(nodeIndex);
	if (ImGui::BeginPopupContextItem("Node Actions"))
	{
		const auto& node = model->GetNodes()[nodeIndex];
		ImGui::TextDisabled("Node: %s", MakeNodeLabel(nodeIndex, node.name).c_str());
		ImGui::Separator();
		if (ImGui::BeginMenu("Add"))
		{
			if (ImGui::MenuItem("Rigid body"))
			{
				auto& value = model->GetVmdlExtensionData().rigidBodies.emplace_back();
				value.nodeIndex = nodeIndex;
				selectedNode = nodeIndex;
				selectedMesh = -1;
				MarkDirty();
			}
			if (ImGui::MenuItem("Collider"))
			{
				auto& value = model->GetVmdlExtensionData().colliders.emplace_back();
				value.nodeIndex = nodeIndex;
				selectedNode = nodeIndex;
				selectedMesh = -1;
				MarkDirty();
			}
			if (ImGui::MenuItem("Spring"))
			{
				auto& value = model->GetVmdlExtensionData().springs.emplace_back();
				value.nodeIndex = nodeIndex;
				selectedNode = nodeIndex;
				selectedMesh = -1;
				MarkDirty();
			}
			if (ImGui::MenuItem("Spring Collider"))
			{
				auto& value = model->GetVmdlExtensionData().springColliders.emplace_back();
				value.nodeIndex = nodeIndex;
				selectedNode = nodeIndex;
				selectedMesh = -1;
				MarkDirty();
			}
			if (ImGui::MenuItem("Trail"))
			{
				auto& value = model->GetVmdlTrailData().trails.emplace_back();
				value.nodeIndex = nodeIndex;
				selectedNode = nodeIndex;
				selectedMesh = -1;
				MarkDirty();
			}
			ImGui::EndMenu();
		}
		ImGui::EndPopup();
	}
	ImGui::PopID();
}

void VmdlEditorScene::DrawViewport()
{
	ImGui::TextUnformatted("3D View");
	ImGui::SameLine();
	if (ImGui::SmallButton("Move")) gizmoOperation = ImGuizmo::TRANSLATE;
	ImGui::SameLine();
	if (ImGui::SmallButton("Rotate")) gizmoOperation = ImGuizmo::ROTATE;
	ImGui::SameLine();
	if (ImGui::SmallButton("Scale")) gizmoOperation = ImGuizmo::SCALE;
	ImGui::Separator();

	ImVec2 available = ImGui::GetContentRegionAvail();
	const float side = std::min(available.x, available.y);
	const ImVec2 imageSize(side, side);
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (available.x - side) * 0.5f);
	ImGui::Image(previewTarget->GetSRV(), imageSize);
	const ImVec2 imageMin = ImGui::GetItemRectMin();
	const ImVec2 imageMax = ImGui::GetItemRectMax();
	const bool imageHovered = ImGui::IsItemHovered();

	if (model && selectedNode >= 0 && selectedNode < static_cast<int>(model->GetNodes().size()))
	{
		Matrix view = editorCamera->GetView();
		Matrix projection = editorCamera->GetProjection();
		const Matrix renderScaleTransform = model->GetRenderScaleTransform();
		Matrix world = model->GetNodes()[selectedNode].worldTransform * renderScaleTransform;
		ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
		ImGuizmo::SetRect(imageMin.x, imageMin.y, imageMax.x - imageMin.x, imageMax.y - imageMin.y);
		if (ImGuizmo::Manipulate(
			&view._11,
			&projection._11,
			static_cast<ImGuizmo::OPERATION>(gizmoOperation),
			ImGuizmo::LOCAL,
			&world._11))
		{
			VMDLModel::Node& node = model->GetNodes()[selectedNode];
			Matrix global = world * renderScaleTransform.Invert();
			Matrix local = node.parent ? global * node.parent->globalTransform.Invert() : global;
			local.Decompose(node.scale, node.rotation, node.position);
			MarkDirty();
			if (animationRecording) RecordSelectedNodeKey();
		}
	}
	if (!imageHovered || ImGuizmo::IsUsing() || ImGuizmo::IsOver()) return;

	const ImGuiIO& io = ImGui::GetIO();
	if (ImGui::IsMouseDragging(ImGuiMouseButton_Right))
	{
		cameraYaw -= io.MouseDelta.x * 0.01f;
		cameraPitch = std::clamp(cameraPitch + io.MouseDelta.y * 0.01f, -1.45f, 1.45f);
	}
	if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
	{
		float panScale = cameraDistance * 0.0015f;
		if (io.KeyShift) panScale *= 10.0f;
		cameraFocusOffset -= editorCamera->GetRight() * io.MouseDelta.x * panScale;
		cameraFocusOffset += editorCamera->GetUp() * io.MouseDelta.y * panScale;
	}
	if (io.MouseWheel != 0.0f)
	{
		if (io.KeyShift) targetCameraDistance *= std::pow(2.0f, -io.MouseWheel);
		else targetCameraDistance -= io.MouseWheel * 0.4f;
		targetCameraDistance = std::clamp(targetCameraDistance, PreviewMinCameraDistance, PreviewMaxCameraDistance);
	}
}

void VmdlEditorScene::DrawProperty()
{
	ImGui::TextUnformatted("Property");
	ImGui::Separator();
	if (!model || selectedNode < 0 || selectedNode >= static_cast<int>(model->GetNodes().size()))
	{
		ImGui::TextDisabled("Select a node or mesh.");
		return;
	}

	VMDLModel::Node& node = model->GetNodes()[selectedNode];
	ImGui::Text("Node: %s", MakeNodeLabel(selectedNode, node.name).c_str());
	bool transformChanged = ImGui::DragFloat3("Local Position", &node.position.x, 0.01f);
	if (ImGui::DragFloat4("Local Rotation", &node.rotation.x, 0.01f))
	{
		node.rotation.Normalize();
		transformChanged = true;
	}
	transformChanged |= ImGui::DragFloat3("Local Scale", &node.scale.x, 0.01f);
	if (transformChanged)
	{
		MarkDirty();
		if (animationRecording) RecordSelectedNodeKey();
	}

	if (selectedMesh >= 0 && selectedMesh < static_cast<int>(model->GetMeshes().size()))
	{
		VMDLModel::Mesh& mesh = model->GetMeshes()[selectedMesh];
		ImGui::SeparatorText("Mesh");
		ImGui::Text("Material: %s", mesh.material->name.c_str());
		ImGui::Text("Vertices: %zu", mesh.vertices.size());
		ImGui::Text("Faces: %zu", mesh.indices.size() / 3);
		if (ImGui::Checkbox("Visible", &mesh.isDraw)) MarkDirty();
	}
	if (!usePbr) ImGui::ColorEdit4("Solid Color", &solidColor.x);

	ImGui::SeparatorText("Attachments");
	ImGui::TextDisabled("Right-click this node in Hierarchy to add attachments.");
	DrawAttachedData(selectedNode);
}

void VmdlEditorScene::DrawAttachedData(int nodeIndex)
{
	auto& data = model->GetVmdlExtensionData();
	int deleteRigidBody = -1;
	for (int i = 0; i < static_cast<int>(data.rigidBodies.size()); ++i)
	{
		auto& value = data.rigidBodies[i];
		if (value.nodeIndex != nodeIndex) continue;
		ImGui::PushID(1000 + i);
		if (ImGui::TreeNode("Rigid body"))
		{
			if (ImGui::InputText("Name", &value.name) ||
				ImGui::DragFloat3("Offset Position", &value.offsetPosition.x, 0.01f) ||
				ImGui::DragFloat3("Offset Rotation", &value.offsetRotation.x, 0.01f) ||
				ImGui::DragFloat("Mass", &value.mass, 0.05f, 0.0f) ||
				ImGui::Checkbox("Kinematic", &value.kinematic)) MarkDirty();
			if (ImGui::Button("Delete")) deleteRigidBody = i;
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	if (deleteRigidBody >= 0)
	{
		data.rigidBodies.erase(data.rigidBodies.begin() + deleteRigidBody);
		MarkDirty();
	}

	int deleteCollider = -1;
	for (int i = 0; i < static_cast<int>(data.colliders.size()); ++i)
	{
		auto& value = data.colliders[i];
		if (value.nodeIndex != nodeIndex) continue;
		ImGui::PushID(2000 + i);
		if (ImGui::TreeNode("Collider"))
		{
			const char* shapes[] = {"Box", "Sphere", "Capsule"};
			bool initialActive = model->GetColliderInitialActive(i);
			bool changed = ImGui::InputText("Name", &value.name);
			PhysicsLayerManager& layerManager = PhysicsLayerManager::Instance();
			const std::string layerPreview = value.layer < 0
				? "-1: Inherit"
				: layerManager.GetLayerDisplayName(static_cast<LayerId>(value.layer));
			if (ImGui::BeginCombo("Layer", layerPreview.c_str()))
			{
				if (ImGui::Selectable("-1: Inherit", value.layer < 0))
				{
					value.layer = -1;
					changed = true;
				}
				for (int layer = 0; layer < EditableLayerCount; ++layer)
				{
					const std::string label = layerManager.GetLayerDisplayName(static_cast<LayerId>(layer));
					if (ImGui::Selectable(label.c_str(), value.layer == layer))
					{
						value.layer = layer;
						changed = true;
					}
				}
				ImGui::EndCombo();
			}
			if (ImGui::Combo("Shape", &value.shape, shapes, IM_ARRAYSIZE(shapes)))
			{
				if (value.shape == 1) value.size.y = value.size.z = value.size.x;
				changed = true;
			}
			Vector3 scaledCenter = model->GetScaledAttachmentVector(value.center);
			Vector3 scaledSize = model->GetScaledAttachmentVector(value.size);
			if (ImGui::DragFloat3("Offset Position", &scaledCenter.x, 0.01f))
			{
				value.center = model->GetUnscaledAttachmentVector(scaledCenter);
				changed = true;
			}
			changed |= ImGui::DragFloat3("Offset Rotation", &value.rotation.x, 0.01f);
			if (value.shape == 1)
			{
				if (ImGui::DragFloat("Radius", &scaledSize.x, 0.01f, 0.001f))
				{
					scaledSize.x = std::max(0.001f, scaledSize.x);
					scaledSize.y = scaledSize.z = scaledSize.x;
					value.size = model->GetUnscaledAttachmentVector(scaledSize);
					changed = true;
				}
			}
			else if (value.shape == 2)
			{
				bool sizeChanged = ImGui::DragFloat("Radius", &scaledSize.x, 0.01f, 0.001f);
				sizeChanged |= ImGui::DragFloat("Height", &scaledSize.y, 0.01f, 0.001f);
				if (sizeChanged)
				{
					scaledSize.x = std::max(0.001f, scaledSize.x);
					scaledSize.y = std::max(0.001f, scaledSize.y);
					value.size = model->GetUnscaledAttachmentVector(scaledSize);
					changed = true;
				}
			}
			else if (ImGui::DragFloat3("Size", &scaledSize.x, 0.01f))
			{
				value.size = model->GetUnscaledAttachmentVector(scaledSize);
				changed = true;
			}
			changed |= ImGui::Checkbox("Trigger", &value.trigger);
			if (changed) MarkDirty();
			if (ImGui::Checkbox("Initially Active", &initialActive))
			{
				model->SetColliderInitialActive(i, initialActive);
				if (previewColliderActive.size() <= static_cast<size_t>(i)) previewColliderActive.resize(i + 1, 1);
				previewColliderActive[i] = initialActive ? 1 : 0;
				MarkDirty();
			}

			if (ImGui::Button("Delete")) deleteCollider = i;
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	if (deleteCollider >= 0)
	{
		data.colliders.erase(data.colliders.begin() + deleteCollider);
		auto& control = model->GetVmdlAnimationControlData();
		if (deleteCollider < static_cast<int>(control.colliderInitialActive.size()))
			control.colliderInitialActive.erase(control.colliderInitialActive.begin() + deleteCollider);
		std::erase_if(control.colliderTracks, [deleteCollider](const auto& track) { return track.colliderIndex == deleteCollider; });
		for (auto& track : control.colliderTracks)
		{
			if (track.colliderIndex > deleteCollider) --track.colliderIndex;
		}
		if (deleteCollider < static_cast<int>(previewColliderActive.size()))
			previewColliderActive.erase(previewColliderActive.begin() + deleteCollider);
		selectedColliderEventTarget = data.colliders.empty() ? 0 : std::min(selectedColliderEventTarget, static_cast<int>(data.colliders.size()) - 1);
		if (timelineEventContextKind == 0) timelineEventContextKind = -1;
		MarkDirty();
	}

	int deleteSpring = -1;
	for (int i = 0; i < static_cast<int>(data.springs.size()); ++i)
	{
		auto& value = data.springs[i];
		if (value.nodeIndex != nodeIndex) continue;
		ImGui::PushID(3000 + i);
		if (ImGui::TreeNode("Spring"))
		{
			if (ImGui::InputText("Name", &value.name) ||
				ImGui::DragFloat3("Offset Position", &value.offsetPosition.x, 0.01f) ||
				ImGui::DragFloat3("Offset Rotation", &value.offsetRotation.x, 0.01f) ||
				ImGui::DragFloat("Stiffness", &value.stiffness, 0.01f, 0.0f, 1.0f) ||
				ImGui::DragFloat("Drag", &value.drag, 0.01f, 0.0f, 1.0f)) MarkDirty();

			if (ImGui::Button("Delete")) deleteSpring = i;
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	if (deleteSpring >= 0)
	{
		data.springs.erase(data.springs.begin() + deleteSpring);
		MarkDirty();
	}

	int deleteSpringCollider = -1;
	for (int i = 0; i < static_cast<int>(data.springColliders.size()); ++i)
	{
		auto& value = data.springColliders[i];
		if (value.nodeIndex != nodeIndex) continue;
		ImGui::PushID(4000 + i);
		if (ImGui::TreeNode("Spring Collider"))
		{
			if (ImGui::InputText("Name", &value.name) ||
				ImGui::DragFloat3("Offset Position", &value.offsetPosition.x, 0.01f) ||
				ImGui::DragFloat("Radius", &value.radius, 0.01f, 0.001f)) MarkDirty();

			if (ImGui::Button("Delete")) deleteSpringCollider = i;
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	if (deleteSpringCollider >= 0)
	{
		data.springColliders.erase(data.springColliders.begin() + deleteSpringCollider);
		MarkDirty();
	}

	auto& trailData = model->GetVmdlTrailData();
	int deleteTrail = -1;
	for (int i = 0; i < static_cast<int>(trailData.trails.size()); ++i)
	{
		auto& value = trailData.trails[i];
		if (value.nodeIndex != nodeIndex) continue;
		ImGui::PushID(5000 + i);
		if (ImGui::TreeNode("Trail"))
		{
			bool initialActive = model->GetTrailInitialActive(i);
			bool changed = ImGui::InputText("Name", &value.name);
			changed |= ImGui::DragFloat3("Root Offset", &value.rootOffset.x, 0.01f);
			changed |= ImGui::DragFloat3("Tip Offset", &value.tipOffset.x, 0.01f);
			changed |= ImGui::ColorEdit4("Color", &value.color.x, ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
			changed |= ImGui::DragFloat("Tip Ratio", &value.tipRatio, 0.01f, 0.0f, 4.0f);
			changed |= ImGui::DragFloat("Life Time", &value.lifeTime, 0.01f, 0.01f, 10.0f, "%.3f sec");
			changed |= ImGui::DragInt("Max Points", &value.maxPoints, 1.0f, 2, 1024);
			changed |= ImGui::DragFloat3("Offset Angle", &value.offsetAngle.x, 0.01f);
			value.tipRatio = std::clamp(value.tipRatio, 0.0f, 4.0f);
			value.lifeTime = std::clamp(value.lifeTime, 0.01f, 10.0f);
			value.maxPoints = std::clamp(value.maxPoints, 2, 1024);
			if (changed) MarkDirty();
			if (ImGui::Checkbox("Initially Active", &initialActive))
			{
				model->SetTrailInitialActive(i, initialActive);
				if (previewTrailActive.size() <= static_cast<size_t>(i)) previewTrailActive.resize(i + 1, 1);
				previewTrailActive[i] = initialActive ? 1 : 0;
				MarkDirty();
			}
			if (ImGui::Button("Delete")) deleteTrail = i;
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	if (deleteTrail >= 0)
	{
		trailData.trails.erase(trailData.trails.begin() + deleteTrail);
		if (deleteTrail < static_cast<int>(trailData.initialActive.size()))
			trailData.initialActive.erase(trailData.initialActive.begin() + deleteTrail);
		std::erase_if(trailData.tracks, [deleteTrail](const auto& track) { return track.trailIndex == deleteTrail; });
		for (auto& track : trailData.tracks)
		{
			if (track.trailIndex > deleteTrail) --track.trailIndex;
		}
		if (deleteTrail < static_cast<int>(previewTrailActive.size()))
			previewTrailActive.erase(previewTrailActive.begin() + deleteTrail);
		selectedTrailEventTarget = trailData.trails.empty() ? 0 : std::min(selectedTrailEventTarget, static_cast<int>(trailData.trails.size()) - 1);
		if (timelineEventContextKind == 2) timelineEventContextKind = -1;
		MarkDirty();
	}
}

void VmdlEditorScene::DrawTimeline()
{
	ImGui::TextUnformatted("Timeline / Animation / IK");
	ImGui::Separator();
	if (!model)
	{
		ImGui::TextDisabled("No model loaded.");
		return;
	}

	auto& animations = model->GetAnimations();
	const char* preview = selectedAnimation >= 0 && selectedAnimation < static_cast<int>(animations.size()) ? animations[selectedAnimation].name.c_str() : "(none)";
	if (ImGui::BeginCombo("Animation", preview))
	{
		for (int i = 0; i < static_cast<int>(animations.size()); ++i)
		{
			if (ImGui::Selectable(animations[i].name.c_str(), selectedAnimation == i))
			{
				selectedAnimation = i;
				animationTime = 0.0f;
				animationPlaying = false;
				ApplyAnimationPreview();
			}
		}
		ImGui::EndCombo();
	}
	if (selectedAnimation < 0 || selectedAnimation >= static_cast<int>(animations.size())) return;

	VMDLModel::Animation& animation = animations[selectedAnimation];

	if (animationRecording)
	{
		ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.8f, 0.0f, 0.0f, 1.0f));
	}

	const bool recordClicked = ImGui::Button(
		ICON_FA_CAMERA "##Record",
		ImVec2(38.0f, 0.0f));

	if (animationRecording)
	{
		ImGui::PopStyleColor(3);
	}

	if (recordClicked) animationRecording = !animationRecording;
	ImGui::SameLine();
	if (ImGui::Button(animationPlaying ? ICON_FA_PAUSE "##Play" : ICON_FA_PLAY "##Play", ImVec2(38.0f, 0.0f))) animationPlaying = !animationPlaying;
	ImGui::SameLine();
	if (ImGui::Button(ICON_FA_STOP "##Stop", ImVec2(38.0f, 0.0f)))
	{
		animationPlaying = false;
		animationTime = 0.0f;
		ApplyAnimationPreview();
		ResetAnimationControlPreview();
	}
	ImGui::SameLine();
	ImGui::Checkbox("Loop", &animationLoop);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(110.0f);
	ImGui::DragFloat("Speed", &playbackSpeed, 0.05f, 0.05f, 4.0f, "x %.2f");
	ImGui::SameLine();
	ImGui::Text("%.3f / %.3f sec", animationTime, animation.secondsLength);
	if (ImGui::Button("Register Key", ImVec2(130.0f, 0.0f))) AddAnimationKey();
	ImGui::SameLine();
	if (selectedNode >= 0 && selectedNode < static_cast<int>(animation.nodeAnims.size()))
	{
		DrawAnimationCurves();
	}
}

void VmdlEditorScene::DrawAnimationCurves()
{
	if (!model || selectedAnimation < 0 || selectedNode < 0) return;
	auto& animation = model->GetAnimations()[selectedAnimation];
	if (selectedNode >= static_cast<int>(animation.nodeAnims.size())) return;
	auto& keys = animation.nodeAnims[selectedNode];
	const float length = std::max(0.001f, animation.secondsLength);
	model->EnsureVmdlIKSettingsCompatibility();
	const auto& ikSettings = model->GetVmdlIKSettings();
	const int footWeightCount = ikSettings.type == 0
		? 0
		: static_cast<int>(std::min(ikSettings.legs.size(), VMDLModel::VmdlIKSettings::MaxLegCount));
	const auto footWeightLabel = [&](int index)
	{
		return ikSettings.legs[index].name.c_str();
	};
	std::vector<VMDLModel::VmdlFootWeightTrack*> footWeightTracks(footWeightCount, nullptr);
	for (int i = 0; i < footWeightCount; ++i)
		footWeightTracks[i] = model->FindFootWeightTrack(animation.name, i);

	struct DopeRow
	{
		const char* label;
		int track;
		bool child;
		int component;
	};
	std::vector<DopeRow> rows;
	const auto addRows = [&rows](const char* label, int track, bool expanded, int componentCount)
	{
		rows.push_back({label, track, false, -1});
		if (!expanded) return;
		constexpr const char* components[] = {"X", "Y", "Z", "W"};
		for (int i = 0; i < componentCount; ++i) rows.push_back({components[i], track, true, i});
	};
	addRows("Position", 0, positionTrackExpanded, 3);
	addRows("Rotation", 1, rotationTrackExpanded, 4);
	addRows("Scale", 2, scaleTrackExpanded, 3);
	const auto hasSelectedKey = [&]()
	{
		if (selectedKeyTrack == 0) return selectedKeyIndex >= 0 && selectedKeyIndex < static_cast<int>(keys.positionKeyframes.size());
		if (selectedKeyTrack == 1) return selectedKeyIndex >= 0 && selectedKeyIndex < static_cast<int>(keys.rotationKeyframes.size());
		if (selectedKeyTrack == 2) return selectedKeyIndex >= 0 && selectedKeyIndex < static_cast<int>(keys.scaleKeyframes.size());
		return false;
	};
	const auto deleteSelectedKey = [&]()
	{
		if (!hasSelectedKey()) return;
		if (selectedKeyTrack == 0) keys.positionKeyframes.erase(keys.positionKeyframes.begin() + selectedKeyIndex);
		else if (selectedKeyTrack == 1) keys.rotationKeyframes.erase(keys.rotationKeyframes.begin() + selectedKeyIndex);
		else keys.scaleKeyframes.erase(keys.scaleKeyframes.begin() + selectedKeyIndex);
		selectedKeyTrack = -1;
		selectedKeyIndex = -1;
		draggingAnimationKey = false;
		draggingNumericTrack = -1;
		draggingNumericComponent = -1;
		MarkDirty();
		ApplyAnimationPreview();
	};
	struct KeyHit
	{
		ImVec2 position;
		int track;
		int index;
	};
	std::vector<KeyHit> hitKeys;

	ImGui::SeparatorText("Dope Sheet");
	ImGui::BeginDisabled(!hasSelectedKey());
	if (ImGui::SmallButton("Delete Selected Key")) deleteSelectedKey();
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (hasSelectedKey() && !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete, false)) deleteSelectedKey();
	const float rulerHeight = 26.0f;
	const float footWeightHeight = 20.0f;
	const float rowHeight = 20.0f;
	const float labelWidth = 250.0f;
	const float footWeightsHeight = footWeightHeight * static_cast<float>(footWeightCount);
	const float rowsTopOffset = rulerHeight + footWeightsHeight;
	const int colliderRowCount = static_cast<int>(model->GetVmdlExtensionData().colliders.size());
	const int trailRowCount = static_cast<int>(model->GetVmdlTrailData().trails.size());
	const int morphRowCount = static_cast<int>(model->GetVmdlExtensionData().morphs.size());
	const int eventRowCount = colliderRowCount + trailRowCount + morphRowCount;
	const float eventRowsTopOffset = rowsTopOffset + rowHeight * static_cast<float>(rows.size());
	const float sheetHeight = eventRowsTopOffset + rowHeight * static_cast<float>(eventRowCount);
	ImGui::InvisibleButton("Animation Dope Sheet", ImVec2(-1.0f, sheetHeight), ImGuiButtonFlags_MouseButtonLeft);
	const ImVec2 sheetMin = ImGui::GetItemRectMin();
	const ImVec2 sheetMax = ImGui::GetItemRectMax();
	const bool hovered = ImGui::IsItemHovered();
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const float timeLeft = sheetMin.x + labelWidth + 8.0f;
	const float timeRight = sheetMax.x - 8.0f;
	const float timeWidth = std::max(1.0f, timeRight - timeLeft);
	const auto timeToX = [&](float seconds) { return timeLeft + std::clamp(seconds / length, 0.0f, 1.0f) * timeWidth; };
	const auto xToTime = [&](float x) { return std::clamp((x - timeLeft) / timeWidth, 0.0f, 1.0f) * length; };

	drawList->AddRectFilled(sheetMin, sheetMax, IM_COL32(42, 42, 45, 255));
	drawList->AddRectFilled(sheetMin, ImVec2(sheetMin.x + labelWidth, sheetMax.y), IM_COL32(52, 52, 56, 255));
	drawList->AddLine(ImVec2(sheetMin.x + labelWidth, sheetMin.y), ImVec2(sheetMin.x + labelWidth, sheetMax.y), IM_COL32(90, 90, 94, 255));
	for (int tick = 0; tick <= 10; ++tick)
	{
		const float ratio = tick / 10.0f;
		const float x = timeLeft + ratio * timeWidth;
		const ImU32 color = tick % 5 == 0 ? IM_COL32(105, 105, 110, 255) : IM_COL32(70, 70, 74, 255);
		drawList->AddLine(ImVec2(x, sheetMin.y + rulerHeight), ImVec2(x, sheetMax.y), color);
		char timeLabel[32]{};
		sprintf_s(timeLabel, "%.2f", length * ratio);
		drawList->AddText(ImVec2(x + 2.0f, sheetMin.y + 4.0f), IM_COL32(185, 185, 190, 255), timeLabel);
	}
	drawList->AddLine(ImVec2(sheetMin.x, sheetMin.y + rulerHeight), ImVec2(sheetMax.x, sheetMin.y + rulerHeight), IM_COL32(95, 95, 100, 255));
	const auto weightColor = [](float weight)
	{
		const float value = std::clamp(std::abs(weight), 0.0f, 1.0f);
		const ImVec4 stops[] = {
			ImVec4(0.0f, 0.0f, 0.0f, 1.0f), ImVec4(0.0f, 0.18f, 0.85f, 1.0f),
			ImVec4(1.0f, 0.9f, 0.0f, 1.0f), ImVec4(1.0f, 0.42f, 0.0f, 1.0f), ImVec4(0.95f, 0.02f, 0.0f, 1.0f)};
		const float scaled = value * 4.0f;
		const int index = std::min(static_cast<int>(scaled), 3);
		const float t = scaled - static_cast<float>(index);
		ImVec4 color(
			std::lerp(stops[index].x, stops[index + 1].x, t),
			std::lerp(stops[index].y, stops[index + 1].y, t),
			std::lerp(stops[index].z, stops[index + 1].z, t), 1.0f);
		if (weight < 0.0f)
		{
			color.x *= 0.45f;
			color.y = std::min(1.0f, color.y + 0.28f);
			color.z = std::min(1.0f, color.z + 0.35f);
		}
		return ImGui::ColorConvertFloat4ToU32(color);
	};
	const float weightTop = sheetMin.y + rulerHeight;
	const float weightBottom = weightTop + footWeightsHeight;
	for (int footIndex = 0; footIndex < footWeightCount; ++footIndex)
	{
		const float rowTop = weightTop + footIndex * footWeightHeight;
		const float rowBottom = rowTop + footWeightHeight;
		drawList->AddRectFilled(ImVec2(sheetMin.x, rowTop), ImVec2(sheetMin.x + labelWidth, rowBottom), IM_COL32(45, 45, 48, 255));
		std::string label = std::string("Foot Weight / ") + footWeightLabel(footIndex);
		drawList->AddText(ImVec2(sheetMin.x + 8.0f, rowTop + 2.0f), IM_COL32(210, 210, 215, 255), label.c_str());
		const ImVec2 negativeButtonMin(sheetMin.x + labelWidth - 42.0f, rowTop + 2.0f);
		const ImVec2 negativeButtonMax(sheetMin.x + labelWidth - 24.0f, rowBottom - 2.0f);
		const ImVec2 applyButtonMin(sheetMin.x + labelWidth - 22.0f, rowTop + 2.0f);
		const ImVec2 applyButtonMax(sheetMin.x + labelWidth - 4.0f, rowBottom - 2.0f);
		const bool negativeButtonHovered = ImGui::IsMouseHoveringRect(negativeButtonMin, negativeButtonMax);
		const bool applyButtonHovered = ImGui::IsMouseHoveringRect(applyButtonMin, applyButtonMax);
		drawList->AddRectFilled(
			negativeButtonMin,
			negativeButtonMax,
			negativeButtonHovered ? IM_COL32(185, 105, 125, 255) : IM_COL32(75, 78, 85, 255),
			3.0f);
		drawList->AddRect(negativeButtonMin, negativeButtonMax, IM_COL32(130, 135, 145, 255), 3.0f);
		drawList->AddText(ImVec2(negativeButtonMin.x + 5.0f, negativeButtonMin.y), IM_COL32(240, 240, 245, 255), "N");
		drawList->AddRectFilled(
			applyButtonMin,
			applyButtonMax,
			applyButtonHovered ? IM_COL32(100, 150, 215, 255) : IM_COL32(75, 78, 85, 255),
			3.0f);
		drawList->AddRect(applyButtonMin, applyButtonMax, IM_COL32(130, 135, 145, 255), 3.0f);
		drawList->AddText(ImVec2(applyButtonMin.x + 5.0f, applyButtonMin.y), IM_COL32(240, 240, 245, 255), "A");
		drawList->AddRectFilled(ImVec2(timeLeft, rowTop + 1.0f), ImVec2(timeRight, rowBottom - 1.0f), IM_COL32(0, 0, 0, 255));
		auto* track = footWeightTracks[footIndex];
		if (track && !track->weights.empty())
		{
			const float sampleWidth = timeWidth / static_cast<float>(track->weights.size());
			for (int i = 0; i < static_cast<int>(track->weights.size()); ++i)
			{
				const float x0 = timeLeft + i * sampleWidth;
				const float x1 = timeLeft + (i + 1) * sampleWidth + 1.0f;
				drawList->AddRectFilled(ImVec2(x0, rowTop + 1.0f), ImVec2(x1, rowBottom - 1.0f), weightColor(track->weights[i]));
			}
		}
		drawList->AddLine(ImVec2(sheetMin.x, rowBottom), ImVec2(sheetMax.x, rowBottom), IM_COL32(95, 95, 100, 255));
	}

	const ImU32 trackColors[] = {IM_COL32(255, 180, 65, 255), IM_COL32(100, 190, 255, 255), IM_COL32(110, 225, 120, 255)};
	const auto numericValue = [&](int track, int component)
	{
		if (track == 0)
		{
			if (selectedKeyTrack == track && selectedKeyIndex >= 0 && selectedKeyIndex < static_cast<int>(keys.positionKeyframes.size())) return (&keys.positionKeyframes[selectedKeyIndex].value.x)[component];
			return (&model->GetNodes()[selectedNode].position.x)[component];
		}
		if (track == 1)
		{
			if (selectedKeyTrack == track && selectedKeyIndex >= 0 && selectedKeyIndex < static_cast<int>(keys.rotationKeyframes.size())) return (&keys.rotationKeyframes[selectedKeyIndex].value.x)[component];
			return (&model->GetNodes()[selectedNode].rotation.x)[component];
		}
		if (selectedKeyTrack == track && selectedKeyIndex >= 0 && selectedKeyIndex < static_cast<int>(keys.scaleKeyframes.size())) return (&keys.scaleKeyframes[selectedKeyIndex].value.x)[component];
		return (&model->GetNodes()[selectedNode].scale.x)[component];
	};
	for (int row = 0; row < static_cast<int>(rows.size()); ++row)
	{
		const float y0 = sheetMin.y + rowsTopOffset + row * rowHeight;
		const float centerY = y0 + rowHeight * 0.5f;
		if ((row & 1) != 0) drawList->AddRectFilled(ImVec2(sheetMin.x, y0), ImVec2(sheetMax.x, y0 + rowHeight), IM_COL32(48, 48, 51, 255));
		drawList->AddLine(ImVec2(sheetMin.x, y0 + rowHeight), ImVec2(sheetMax.x, y0 + rowHeight), IM_COL32(61, 61, 65, 255));
		const float indent = rows[row].child ? 28.0f : 8.0f;
		if (!rows[row].child)
		{
			const bool expanded = rows[row].track == 0 ? positionTrackExpanded : rows[row].track == 1 ? rotationTrackExpanded : scaleTrackExpanded;
			drawList->AddText(ImVec2(sheetMin.x + 7.0f, y0 + 2.0f), trackColors[rows[row].track], expanded ? "v" : ">");
		}
		drawList->AddText(ImVec2(sheetMin.x + indent, y0 + 2.0f), rows[row].child ? IM_COL32(205, 205, 210, 255) : IM_COL32(240, 240, 242, 255), rows[row].label);
		if (rows[row].child)
		{
			char valueText[32]{};
			sprintf_s(valueText, "%.3f", numericValue(rows[row].track, rows[row].component));
			drawList->AddRectFilled(ImVec2(sheetMin.x + 92.0f, y0 + 1.0f), ImVec2(sheetMin.x + labelWidth - 7.0f, y0 + rowHeight - 1.0f), IM_COL32(63, 63, 67, 255), 3.0f);
			drawList->AddText(ImVec2(sheetMin.x + 104.0f, y0 + 2.0f), IM_COL32(220, 220, 225, 255), valueText);
		}

		const auto drawTrackKeys = [&](const auto& trackKeys)
		{
			for (int keyIndex = 0; keyIndex < static_cast<int>(trackKeys.size()); ++keyIndex)
			{
				const ImVec2 position(timeToX(trackKeys[keyIndex].seconds), centerY);
				const bool selected = selectedKeyTrack == rows[row].track && selectedKeyIndex == keyIndex;
				const ImU32 color = selected ? IM_COL32(255, 235, 90, 255) : trackColors[rows[row].track];
				const ImVec2 diamond[] = {
					ImVec2(position.x, position.y - 5.0f), ImVec2(position.x + 5.0f, position.y),
					ImVec2(position.x, position.y + 5.0f), ImVec2(position.x - 5.0f, position.y)};
				drawList->AddConvexPolyFilled(diamond, 4, color);
				drawList->AddPolyline(diamond, 4, IM_COL32(25, 25, 25, 255), ImDrawFlags_Closed, 1.0f);
				hitKeys.push_back({position, rows[row].track, keyIndex});
			}
		};
		if (rows[row].track == 0) drawTrackKeys(keys.positionKeyframes);
		else if (rows[row].track == 1) drawTrackKeys(keys.rotationKeyframes);
		else drawTrackKeys(keys.scaleKeyframes);
	}

	const auto& colliders = model->GetVmdlExtensionData().colliders;
	const auto& trails = model->GetVmdlTrailData().trails;
	const auto& morphs = model->GetVmdlExtensionData().morphs;
	auto& controlData = model->GetVmdlAnimationControlData();
	const auto drawEventDiamond = [&](float seconds, float centerY, ImU32 color)
	{
		const ImVec2 position(timeToX(seconds), centerY);
		const ImVec2 diamond[] = {
			ImVec2(position.x, position.y - 5.0f), ImVec2(position.x + 5.0f, position.y),
			ImVec2(position.x, position.y + 5.0f), ImVec2(position.x - 5.0f, position.y)};
		drawList->AddConvexPolyFilled(diamond, 4, color);
		drawList->AddPolyline(diamond, 4, IM_COL32(25, 25, 25, 255), ImDrawFlags_Closed, 1.0f);
	};
	for (int eventRow = 0; eventRow < eventRowCount; ++eventRow)
	{
		const float y0 = sheetMin.y + eventRowsTopOffset + eventRow * rowHeight;
		const float centerY = y0 + rowHeight * 0.5f;
		if ((eventRow & 1) == 0) drawList->AddRectFilled(ImVec2(sheetMin.x, y0), ImVec2(sheetMax.x, y0 + rowHeight), IM_COL32(48, 48, 51, 255));
		drawList->AddLine(ImVec2(sheetMin.x, y0 + rowHeight), ImVec2(sheetMax.x, y0 + rowHeight), IM_COL32(61, 61, 65, 255));

		if (eventRow < colliderRowCount)
		{
			const int colliderIndex = eventRow;
			const std::string label = "Collider / " + colliders[colliderIndex].name;
			drawList->AddText(ImVec2(sheetMin.x + 8.0f, y0 + 2.0f), IM_COL32(100, 220, 235, 255), label.c_str());
			for (const auto& track : controlData.colliderTracks)
			{
				if (track.animationName != animation.name || track.colliderIndex != colliderIndex) continue;
				for (const auto& key : track.keys)
					drawEventDiamond(key.seconds, centerY, key.value ? IM_COL32(80, 235, 115, 255) : IM_COL32(235, 75, 75, 255));
				break;
			}
		}
		else if (eventRow < colliderRowCount + trailRowCount)
		{
			const int trailIndex = eventRow - colliderRowCount;
			const std::string label = "Trail / " + trails[trailIndex].name;
			drawList->AddText(ImVec2(sheetMin.x + 8.0f, y0 + 2.0f), IM_COL32(255, 155, 55, 255), label.c_str());
			for (const auto& track : model->GetVmdlTrailData().tracks)
			{
				if (track.animationName != animation.name || track.trailIndex != trailIndex) continue;
				for (const auto& key : track.keys)
					drawEventDiamond(key.seconds, centerY, key.value ? IM_COL32(80, 235, 115, 255) : IM_COL32(235, 75, 75, 255));
				break;
			}
		}
		else
		{
			const int morphIndex = eventRow - colliderRowCount - trailRowCount;
			const std::string label = "Morph / " + morphs[morphIndex].name;
			drawList->AddText(ImVec2(sheetMin.x + 8.0f, y0 + 2.0f), IM_COL32(210, 125, 255, 255), label.c_str());
			for (const auto& track : controlData.morphTracks)
			{
				if (track.animationName != animation.name) continue;
				for (const auto& key : track.keys)
				{
					if (key.morphIndex == morphIndex) drawEventDiamond(key.seconds, centerY, IM_COL32(210, 125, 255, 255));
				}
				break;
			}
		}
	}

	const float playheadX = timeToX(animationTime);
	drawList->AddLine(ImVec2(playheadX, sheetMin.y), ImVec2(playheadX, sheetMax.y), IM_COL32(90, 190, 255, 255), 2.0f);
	const ImVec2 playheadTriangle[] = {
		ImVec2(playheadX - 5.0f, sheetMin.y), ImVec2(playheadX + 5.0f, sheetMin.y), ImVec2(playheadX, sheetMin.y + 7.0f)};
	drawList->AddConvexPolyFilled(playheadTriangle, 3, IM_COL32(90, 190, 255, 255));

	if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
	{
		const ImVec2 mouse = ImGui::GetIO().MousePos;
		if (mouse.y >= sheetMin.y + eventRowsTopOffset && mouse.y < sheetMax.y)
		{
			const int eventRow = std::clamp(static_cast<int>((mouse.y - sheetMin.y - eventRowsTopOffset) / rowHeight), 0, eventRowCount - 1);
			if (eventRow < colliderRowCount)
			{
				timelineEventContextKind = 0;
				timelineEventContextTarget = eventRow;
			}
			else if (eventRow < colliderRowCount + trailRowCount)
			{
				timelineEventContextKind = 2;
				timelineEventContextTarget = eventRow - colliderRowCount;
			}
			else
			{
				timelineEventContextKind = 1;
				timelineEventContextTarget = eventRow - colliderRowCount - trailRowCount;
			}
			timelineEventContextTime = xToTime(mouse.x);
			timelineEventContextKey = -1;
			float closest = 9.0f;
			if (timelineEventContextKind == 0)
			{
				for (const auto& track : controlData.colliderTracks)
				{
					if (track.animationName != animation.name || track.colliderIndex != timelineEventContextTarget) continue;
					for (int i = 0; i < static_cast<int>(track.keys.size()); ++i)
					{
						const float distance = std::abs(mouse.x - timeToX(track.keys[i].seconds));
						if (distance >= closest) continue;
						closest = distance;
						timelineEventContextKey = i;
					}
					break;
				}
			}
			else if (timelineEventContextKind == 1)
			{
				for (const auto& track : controlData.morphTracks)
				{
					if (track.animationName != animation.name) continue;
					for (int i = 0; i < static_cast<int>(track.keys.size()); ++i)
					{
						if (track.keys[i].morphIndex != timelineEventContextTarget) continue;
						const float distance = std::abs(mouse.x - timeToX(track.keys[i].seconds));
						if (distance >= closest) continue;
						closest = distance;
						timelineEventContextKey = i;
					}
					break;
				}
			}
			else
			{
				for (const auto& track : model->GetVmdlTrailData().tracks)
				{
					if (track.animationName != animation.name || track.trailIndex != timelineEventContextTarget) continue;
					for (int i = 0; i < static_cast<int>(track.keys.size()); ++i)
					{
						const float distance = std::abs(mouse.x - timeToX(track.keys[i].seconds));
						if (distance >= closest) continue;
						closest = distance;
						timelineEventContextKey = i;
					}
					break;
				}
			}
			ImGui::OpenPopup("Timeline Event Key");
		}
	}
	if (ImGui::BeginPopup("Timeline Event Key"))
	{
		const auto byTime = [](const auto& left, const auto& right) { return left.seconds < right.seconds; };
		if (timelineEventContextKind == 0 && timelineEventContextTarget >= 0 && timelineEventContextTarget < colliderRowCount)
		{
			ImGui::Text("Collider: %s", colliders[timelineEventContextTarget].name.c_str());
			VMDLModel::VmdlColliderAnimationTrack* track = nullptr;
			for (auto& candidate : controlData.colliderTracks)
			{
				if (candidate.animationName == animation.name && candidate.colliderIndex == timelineEventContextTarget)
				{
					track = &candidate;
					break;
				}
			}
			if (track && timelineEventContextKey >= 0 && timelineEventContextKey < static_cast<int>(track->keys.size()))
			{
				auto& key = track->keys[timelineEventContextKey];
				bool changed = ImGui::DragFloat("Time", &key.seconds, 0.01f, 0.0f, length, "%.3f sec");
				changed |= ImGui::Checkbox("Active", &key.value);
				if (changed)
				{
					const float editedSeconds = key.seconds;
					std::sort(track->keys.begin(), track->keys.end(), byTime);
					for (int i = 0; i < static_cast<int>(track->keys.size()); ++i)
					{
						if (std::abs(track->keys[i].seconds - editedSeconds) < 0.0001f) timelineEventContextKey = i;
					}
					MarkDirty();
					ApplyAnimationPreview();
				}
				if (ImGui::Button("Delete Key"))
				{
					track->keys.erase(track->keys.begin() + timelineEventContextKey);
					MarkDirty();
					ApplyAnimationPreview();
					ImGui::CloseCurrentPopup();
				}
			}
			else
			{
				if (ImGui::MenuItem("Add ON Key"))
				{
					auto& newTrack = model->GetOrCreateColliderAnimationTrack(animation.name, timelineEventContextTarget);
					newTrack.keys.push_back({timelineEventContextTime, true});
					std::sort(newTrack.keys.begin(), newTrack.keys.end(), byTime);
					MarkDirty();
					ApplyAnimationPreview();
				}
				if (ImGui::MenuItem("Add OFF Key"))
				{
					auto& newTrack = model->GetOrCreateColliderAnimationTrack(animation.name, timelineEventContextTarget);
					newTrack.keys.push_back({timelineEventContextTime, false});
					std::sort(newTrack.keys.begin(), newTrack.keys.end(), byTime);
					MarkDirty();
					ApplyAnimationPreview();
				}
			}
		}
		else if (timelineEventContextKind == 1 && timelineEventContextTarget >= 0 && timelineEventContextTarget < morphRowCount)
		{
			ImGui::Text("Morph: %s", morphs[timelineEventContextTarget].name.c_str());
			VMDLModel::VmdlMorphAnimationTrack* track = nullptr;
			for (auto& candidate : controlData.morphTracks)
			{
				if (candidate.animationName == animation.name)
				{
					track = &candidate;
					break;
				}
			}
			if (track && timelineEventContextKey >= 0 && timelineEventContextKey < static_cast<int>(track->keys.size()))
			{
				auto& key = track->keys[timelineEventContextKey];
				if (ImGui::DragFloat("Time", &key.seconds, 0.01f, 0.0f, length, "%.3f sec"))
				{
					const float editedSeconds = key.seconds;
					std::sort(track->keys.begin(), track->keys.end(), byTime);
					for (int i = 0; i < static_cast<int>(track->keys.size()); ++i)
					{
						if (std::abs(track->keys[i].seconds - editedSeconds) < 0.0001f && track->keys[i].morphIndex == timelineEventContextTarget) timelineEventContextKey = i;
					}
					MarkDirty();
					ApplyAnimationPreview();
				}
				if (ImGui::Button("Delete Key"))
				{
					track->keys.erase(track->keys.begin() + timelineEventContextKey);
					MarkDirty();
					ApplyAnimationPreview();
					ImGui::CloseCurrentPopup();
				}
			}
			else if (ImGui::MenuItem("Add Morph Key"))
			{
				auto& newTrack = model->GetOrCreateMorphAnimationTrack(animation.name);
				newTrack.keys.push_back({timelineEventContextTime, timelineEventContextTarget});
				std::sort(newTrack.keys.begin(), newTrack.keys.end(), byTime);
				MarkDirty();
				ApplyAnimationPreview();
			}
		}
		else if (timelineEventContextKind == 2 && timelineEventContextTarget >= 0 && timelineEventContextTarget < trailRowCount)
		{
			ImGui::Text("Trail: %s", trails[timelineEventContextTarget].name.c_str());
			VMDLModel::VmdlTrailAnimationTrack* track = nullptr;
			for (auto& candidate : model->GetVmdlTrailData().tracks)
			{
				if (candidate.animationName == animation.name && candidate.trailIndex == timelineEventContextTarget)
				{
					track = &candidate;
					break;
				}
			}
			if (track && timelineEventContextKey >= 0 && timelineEventContextKey < static_cast<int>(track->keys.size()))
			{
				auto& key = track->keys[timelineEventContextKey];
				bool changed = ImGui::DragFloat("Time", &key.seconds, 0.01f, 0.0f, length, "%.3f sec");
				changed |= ImGui::Checkbox("Active", &key.value);
				if (changed)
				{
					const float editedSeconds = key.seconds;
					std::sort(track->keys.begin(), track->keys.end(), byTime);
					for (int i = 0; i < static_cast<int>(track->keys.size()); ++i)
					{
						if (std::abs(track->keys[i].seconds - editedSeconds) < 0.0001f) timelineEventContextKey = i;
					}
					MarkDirty();
					ApplyAnimationPreview();
				}
				if (ImGui::Button("Delete Key"))
				{
					track->keys.erase(track->keys.begin() + timelineEventContextKey);
					MarkDirty();
					ApplyAnimationPreview();
					ImGui::CloseCurrentPopup();
				}
			}
			else
			{
				if (ImGui::MenuItem("Add ON Key"))
				{
					auto& newTrack = model->GetOrCreateTrailAnimationTrack(animation.name, timelineEventContextTarget);
					newTrack.keys.push_back({timelineEventContextTime, true});
					std::sort(newTrack.keys.begin(), newTrack.keys.end(), byTime);
					MarkDirty();
					ApplyAnimationPreview();
				}
				if (ImGui::MenuItem("Add OFF Key"))
				{
					auto& newTrack = model->GetOrCreateTrailAnimationTrack(animation.name, timelineEventContextTarget);
					newTrack.keys.push_back({timelineEventContextTime, false});
					std::sort(newTrack.keys.begin(), newTrack.keys.end(), byTime);
					MarkDirty();
					ApplyAnimationPreview();
				}
			}
		}
		ImGui::EndPopup();
	}

	if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		const ImVec2 mouse = ImGui::GetIO().MousePos;
		const bool clickedNegativeButton = mouse.x >= sheetMin.x + labelWidth - 42.0f && mouse.x <= sheetMin.x + labelWidth - 24.0f;
		const bool clickedApplyButton = mouse.x >= sheetMin.x + labelWidth - 22.0f && mouse.x <= sheetMin.x + labelWidth - 4.0f;
		if ((clickedNegativeButton || clickedApplyButton) && mouse.y >= weightTop && mouse.y < weightBottom)
		{
			const int footIndex = std::clamp(static_cast<int>((mouse.y - weightTop) / footWeightHeight), 0, footWeightCount - 1);
			auto& track = model->GetOrCreateFootWeightTrack(animation.name, footIndex);
			const int sampleCount = std::max(2, static_cast<int>(std::ceil(length * track.sampleRate)) + 1);
			track.weights.assign(sampleCount, clickedNegativeButton ? -1.0f : 1.0f);
			MarkDirty();
			ApplyAnimationPreview();
			return;
		}
		if (mouse.x >= timeLeft && mouse.y >= weightTop && mouse.y < weightBottom)
		{
			paintingFootWeight = true;
			paintingFootWeightIndex = std::clamp(static_cast<int>((mouse.y - weightTop) / footWeightHeight), 0, footWeightCount - 1);
		}
		else if (mouse.x < sheetMin.x + labelWidth && mouse.y >= sheetMin.y + rowsTopOffset && mouse.y < sheetMin.y + eventRowsTopOffset)
		{
			const int row = std::clamp(static_cast<int>((mouse.y - sheetMin.y - rowsTopOffset) / rowHeight), 0, static_cast<int>(rows.size()) - 1);
			if (!rows[row].child)
			{
				if (rows[row].track == 0) positionTrackExpanded = !positionTrackExpanded;
				else if (rows[row].track == 1) rotationTrackExpanded = !rotationTrackExpanded;
				else scaleTrackExpanded = !scaleTrackExpanded;
			}
			else if (mouse.x >= sheetMin.x + 92.0f)
			{
				draggingNumericTrack = rows[row].track;
				draggingNumericComponent = rows[row].component;
			}
			return;
		}
		if (paintingFootWeight) return;
		int hitTrack = -1;
		int hitIndex = -1;
		float closest = 9.0f;
		for (const KeyHit& hit : hitKeys)
		{
			const float distance = std::abs(mouse.x - hit.position.x) + std::abs(mouse.y - hit.position.y);
			if (distance >= closest) continue;
			closest = distance;
			hitTrack = hit.track;
			hitIndex = hit.index;
		}
		if (hitTrack >= 0)
		{
			selectedKeyTrack = hitTrack;
			selectedKeyIndex = hitIndex;
			draggingAnimationKey = true;
		}
		else if (mouse.x >= timeLeft)
		{
			animationPlaying = false;
			scrubbingAnimationTime = true;
			animationTime = xToTime(mouse.x);
			ApplyAnimationPreview();
		}
	}
	if (paintingFootWeight && ImGui::IsMouseDown(ImGuiMouseButton_Left))
	{
		if (paintingFootWeightIndex < 0 || paintingFootWeightIndex >= footWeightCount) return;
		auto* footWeightTrack = footWeightTracks[paintingFootWeightIndex];
		if (!footWeightTrack) footWeightTrack = &model->GetOrCreateFootWeightTrack(animation.name, paintingFootWeightIndex);
		const int sampleCount = std::max(2, static_cast<int>(std::ceil(length * footWeightTrack->sampleRate)) + 1);
		if (footWeightTrack->weights.size() != sampleCount) footWeightTrack->weights.resize(sampleCount, 0.0f);
		const float ratio = std::clamp((ImGui::GetIO().MousePos.x - timeLeft) / timeWidth, 0.0f, 1.0f);
		const int center = std::clamp(static_cast<int>(ratio * (sampleCount - 1)), 0, sampleCount - 1);
		const int radius = std::max(1, static_cast<int>(footWeightTrack->sampleRate * 0.05f));
		const bool erase = ImGui::GetIO().KeyShift;
		const float amount = ImGui::GetIO().DeltaTime * 1.8f;
		for (int offset = -radius; offset <= radius; ++offset)
		{
			const int index = center + offset;
			if (index < 0 || index >= sampleCount) continue;
			const float falloff = 1.0f - std::abs(static_cast<float>(offset)) / static_cast<float>(radius + 1);
			float& weight = footWeightTrack->weights[index];
			if (erase)
			{
				const float eraseAmount = amount * falloff;
				if (weight > 0.0f) weight = std::max(0.0f, weight - eraseAmount);
				else weight = std::min(0.0f, weight + eraseAmount);
			}
			else weight = std::clamp(weight + amount * falloff, 0.0f, 1.0f);
		}
		MarkDirty();
	}
	if (paintingFootWeight && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
	{
		paintingFootWeight = false;
		paintingFootWeightIndex = -1;
	}
	if (scrubbingAnimationTime && ImGui::IsMouseDown(ImGuiMouseButton_Left))
	{
		animationPlaying = false;
		animationTime = xToTime(ImGui::GetIO().MousePos.x);
		ApplyAnimationPreview();
	}
	if (scrubbingAnimationTime && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) scrubbingAnimationTime = false;
	if (draggingNumericTrack >= 0 && ImGui::IsMouseDown(ImGuiMouseButton_Left))
	{
		const float delta = ImGui::GetIO().MouseDelta.x * 0.01f;
		VMDLModel::Node& node = model->GetNodes()[selectedNode];
		if (draggingNumericTrack == 0)
		{
			if (selectedKeyTrack == 0 && selectedKeyIndex >= 0 && selectedKeyIndex < static_cast<int>(keys.positionKeyframes.size())) (&keys.positionKeyframes[selectedKeyIndex].value.x)[draggingNumericComponent] += delta;
			else (&node.position.x)[draggingNumericComponent] += delta;
		}
		else if (draggingNumericTrack == 1)
		{
			if (selectedKeyTrack == 1 && selectedKeyIndex >= 0 && selectedKeyIndex < static_cast<int>(keys.rotationKeyframes.size()))
			{
				(&keys.rotationKeyframes[selectedKeyIndex].value.x)[draggingNumericComponent] += delta;
				keys.rotationKeyframes[selectedKeyIndex].value.Normalize();
			}
			else
			{
				(&node.rotation.x)[draggingNumericComponent] += delta;
				node.rotation.Normalize();
			}
		}
		else
		{
			if (selectedKeyTrack == 2 && selectedKeyIndex >= 0 && selectedKeyIndex < static_cast<int>(keys.scaleKeyframes.size())) (&keys.scaleKeyframes[selectedKeyIndex].value.x)[draggingNumericComponent] += delta;
			else (&node.scale.x)[draggingNumericComponent] += delta;
		}
		MarkDirty();
		if (selectedKeyTrack == draggingNumericTrack) ApplyAnimationPreview();
		else if (animationRecording) RecordSelectedNodeKey();
	}
	if (draggingNumericTrack >= 0 && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
	{
		draggingNumericTrack = -1;
		draggingNumericComponent = -1;
	}
	if (draggingAnimationKey && ImGui::IsMouseDown(ImGuiMouseButton_Left))
	{
		const float seconds = xToTime(ImGui::GetIO().MousePos.x);
		if (selectedKeyTrack == 0 && selectedKeyIndex < static_cast<int>(keys.positionKeyframes.size())) keys.positionKeyframes[selectedKeyIndex].seconds = seconds;
		else if (selectedKeyTrack == 1 && selectedKeyIndex < static_cast<int>(keys.rotationKeyframes.size())) keys.rotationKeyframes[selectedKeyIndex].seconds = seconds;
		else if (selectedKeyTrack == 2 && selectedKeyIndex < static_cast<int>(keys.scaleKeyframes.size())) keys.scaleKeyframes[selectedKeyIndex].seconds = seconds;
		animationTime = seconds;
		animationPlaying = false;
		MarkDirty();
		ApplyAnimationPreview();
	}
	if (draggingAnimationKey && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
	{
		const float selectedTime = animationTime;
		const auto byTime = [](const auto& left, const auto& right) { return left.seconds < right.seconds; };
		if (selectedKeyTrack == 0) std::sort(keys.positionKeyframes.begin(), keys.positionKeyframes.end(), byTime);
		else if (selectedKeyTrack == 1) std::sort(keys.rotationKeyframes.begin(), keys.rotationKeyframes.end(), byTime);
		else if (selectedKeyTrack == 2) std::sort(keys.scaleKeyframes.begin(), keys.scaleKeyframes.end(), byTime);
		const auto findSelected = [&](const auto& trackKeys)
		{
			int closestIndex = -1;
			float closestDistance = FLT_MAX;
			for (int i = 0; i < static_cast<int>(trackKeys.size()); ++i)
			{
				const float distance = std::abs(trackKeys[i].seconds - selectedTime);
				if (distance >= closestDistance) continue;
				closestDistance = distance;
				closestIndex = i;
			}
			return closestIndex;
		};
		if (selectedKeyTrack == 0) selectedKeyIndex = findSelected(keys.positionKeyframes);
		else if (selectedKeyTrack == 1) selectedKeyIndex = findSelected(keys.rotationKeyframes);
		else if (selectedKeyTrack == 2) selectedKeyIndex = findSelected(keys.scaleKeyframes);
		draggingAnimationKey = false;
	}
}

void VmdlEditorScene::ApplyAnimationPreview()
{
	if (!model || selectedAnimation < 0 || selectedAnimation >= static_cast<int>(model->GetAnimations().size())) return;
	const auto& animation = model->GetAnimations()[selectedAnimation];
	auto& nodes = model->GetNodes();
	const size_t count = std::min(nodes.size(), animation.nodeAnims.size());
	for (size_t i = 0; i < count; ++i)
	{
		const auto& keys = animation.nodeAnims[i];
		nodes[i].position = EvaluateVectorKeys(keys.positionKeyframes, animationTime, nodes[i].position);
		nodes[i].rotation = EvaluateQuaternionKeys(keys.rotationKeyframes, animationTime, nodes[i].rotation);
		nodes[i].scale = EvaluateVectorKeys(keys.scaleKeyframes, animationTime, nodes[i].scale);
	}
	if (previewColliderActive.size() != model->GetVmdlExtensionData().colliders.size())
		previewColliderActive.resize(model->GetVmdlExtensionData().colliders.size(), 1);
	for (int i = 0; i < static_cast<int>(previewColliderActive.size()); ++i)
		previewColliderActive[i] = model->EvaluateColliderActive(selectedAnimation, animationTime, i) ? 1 : 0;
	previewTrailActive.resize(model->GetVmdlTrailData().trails.size(), 1);
	for (int i = 0; i < static_cast<int>(previewTrailActive.size()); ++i)
		previewTrailActive[i] = model->EvaluateTrailActive(selectedAnimation, animationTime, i) ? 1 : 0;
	model->ApplyMorphAnimation(selectedAnimation, animationTime);
}

void VmdlEditorScene::ResetAnimationControlPreview()
{
	if (!model) return;
	previewColliderActive.resize(model->GetVmdlExtensionData().colliders.size(), 1);
	for (int i = 0; i < static_cast<int>(previewColliderActive.size()); ++i)
		previewColliderActive[i] = model->GetColliderInitialActive(i) ? 1 : 0;
	previewTrailActive.resize(model->GetVmdlTrailData().trails.size(), 1);
	for (int i = 0; i < static_cast<int>(previewTrailActive.size()); ++i)
		previewTrailActive[i] = model->GetTrailInitialActive(i) ? 1 : 0;
	model->RestoreRuntimeMorphVisibility();
}

void VmdlEditorScene::DrawAnimationEventEditor()
{
	if (!model || selectedAnimation < 0 || selectedAnimation >= static_cast<int>(model->GetAnimations().size())) return;
	auto& animation = model->GetAnimations()[selectedAnimation];
	auto& colliders = model->GetVmdlExtensionData().colliders;
	auto& morphs = model->GetVmdlExtensionData().morphs;
	auto& controlData = model->GetVmdlAnimationControlData();
	const auto byTime = [](const auto& left, const auto& right) { return left.seconds < right.seconds; };

	ImGui::SeparatorText("Animation Events");

	if (ImGui::TreeNodeEx("Collider Active", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (colliders.empty()) ImGui::TextDisabled("No Collider is registered in this VMDL.");
		else
		{
			selectedColliderEventTarget = std::clamp(selectedColliderEventTarget, 0, static_cast<int>(colliders.size()) - 1);
			if (ImGui::BeginCombo("Collider", colliders[selectedColliderEventTarget].name.c_str()))
			{
				for (int i = 0; i < static_cast<int>(colliders.size()); ++i)
				{
					if (ImGui::Selectable(colliders[i].name.c_str(), selectedColliderEventTarget == i)) selectedColliderEventTarget = i;
				}
				ImGui::EndCombo();
			}
			bool initialActive = model->GetColliderInitialActive(selectedColliderEventTarget);
			if (ImGui::Checkbox("Initial Active", &initialActive))
			{
				model->SetColliderInitialActive(selectedColliderEventTarget, initialActive);
				MarkDirty();
				ApplyAnimationPreview();
			}
			if (ImGui::Button("Add ON Key"))
			{
				auto& track = model->GetOrCreateColliderAnimationTrack(animation.name, selectedColliderEventTarget);
				track.keys.push_back({animationTime, true});
				std::sort(track.keys.begin(), track.keys.end(), byTime);
				MarkDirty();
				ApplyAnimationPreview();
			}
			ImGui::SameLine();
			if (ImGui::Button("Add OFF Key"))
			{
				auto& track = model->GetOrCreateColliderAnimationTrack(animation.name, selectedColliderEventTarget);
				track.keys.push_back({animationTime, false});
				std::sort(track.keys.begin(), track.keys.end(), byTime);
				MarkDirty();
				ApplyAnimationPreview();
			}

			VMDLModel::VmdlColliderAnimationTrack* selectedTrack = nullptr;
			for (auto& track : controlData.colliderTracks)
			{
				if (track.animationName == animation.name && track.colliderIndex == selectedColliderEventTarget)
				{
					selectedTrack = &track;
					break;
				}
			}
			if (selectedTrack)
			{
				int removeIndex = -1;
				bool sortKeys = false;
				bool valueChanged = false;
				for (int i = 0; i < static_cast<int>(selectedTrack->keys.size()); ++i)
				{
					auto& key = selectedTrack->keys[i];
					ImGui::PushID(i);
					ImGui::SetNextItemWidth(150.0f);
					if (ImGui::DragFloat("##Time", &key.seconds, 0.01f, 0.0f, animation.secondsLength, "%.3f sec")) sortKeys = true;
					ImGui::SameLine();
					if (ImGui::Checkbox("Active", &key.value)) valueChanged = true;
					ImGui::SameLine();
					if (ImGui::SmallButton("Delete")) removeIndex = i;
					ImGui::PopID();
				}
				if (removeIndex >= 0)
				{
					selectedTrack->keys.erase(selectedTrack->keys.begin() + removeIndex);
					MarkDirty();
				}
				if (sortKeys)
				{
					std::sort(selectedTrack->keys.begin(), selectedTrack->keys.end(), byTime);
					MarkDirty();
				}
				if (valueChanged) MarkDirty();
				if (removeIndex >= 0 || sortKeys || valueChanged) ApplyAnimationPreview();
			}
		}
		ImGui::TreePop();
	}

	if (ImGui::TreeNodeEx("Morph Apply", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (morphs.empty()) ImGui::TextDisabled("No Morph is registered in this VMDL.");
		else
		{
			selectedMorphEventTarget = std::clamp(selectedMorphEventTarget, 0, static_cast<int>(morphs.size()) - 1);
			if (ImGui::BeginCombo("Morph", morphs[selectedMorphEventTarget].name.c_str()))
			{
				for (int i = 0; i < static_cast<int>(morphs.size()); ++i)
				{
					if (ImGui::Selectable(morphs[i].name.c_str(), selectedMorphEventTarget == i)) selectedMorphEventTarget = i;
				}
				ImGui::EndCombo();
			}
			if (ImGui::Button("Add Morph Key"))
			{
				auto& track = model->GetOrCreateMorphAnimationTrack(animation.name);
				track.keys.push_back({animationTime, selectedMorphEventTarget});
				std::sort(track.keys.begin(), track.keys.end(), byTime);
				MarkDirty();
				ApplyAnimationPreview();
			}

			VMDLModel::VmdlMorphAnimationTrack* selectedTrack = nullptr;
			for (auto& track : controlData.morphTracks)
			{
				if (track.animationName == animation.name)
				{
					selectedTrack = &track;
					break;
				}
			}
			if (selectedTrack)
			{
				int removeIndex = -1;
				bool changed = false;
				for (int i = 0; i < static_cast<int>(selectedTrack->keys.size()); ++i)
				{
					auto& key = selectedTrack->keys[i];
					ImGui::PushID(i);
					ImGui::SetNextItemWidth(150.0f);
					if (ImGui::DragFloat("##Time", &key.seconds, 0.01f, 0.0f, animation.secondsLength, "%.3f sec")) changed = true;
					ImGui::SameLine();
					const char* keyMorphName = key.morphIndex >= 0 && key.morphIndex < static_cast<int>(morphs.size()) ? morphs[key.morphIndex].name.c_str() : "(missing)";
					ImGui::SetNextItemWidth(220.0f);
					if (ImGui::BeginCombo("##Morph", keyMorphName))
					{
						for (int morphIndex = 0; morphIndex < static_cast<int>(morphs.size()); ++morphIndex)
						{
							if (ImGui::Selectable(morphs[morphIndex].name.c_str(), key.morphIndex == morphIndex))
							{
								key.morphIndex = morphIndex;
								changed = true;
							}
						}
						ImGui::EndCombo();
					}
					ImGui::SameLine();
					if (ImGui::SmallButton("Delete")) removeIndex = i;
					ImGui::PopID();
				}
				if (removeIndex >= 0) selectedTrack->keys.erase(selectedTrack->keys.begin() + removeIndex);
				if (changed) std::sort(selectedTrack->keys.begin(), selectedTrack->keys.end(), byTime);
				if (changed || removeIndex >= 0)
				{
					MarkDirty();
					ApplyAnimationPreview();
				}
			}
		}
		ImGui::TreePop();
	}
}

void VmdlEditorScene::DrawIkSettings()
{
	ImGui::TextUnformatted("IK Settings");
	ImGui::Separator();
	if (!model)
	{
		ImGui::TextDisabled("Nothing");
		return;
	}

	model->EnsureVmdlIKSettingsCompatibility();
	auto& settings = model->GetVmdlIKSettings();
	const char* types[] = {"None", "Human Foot IK", "Quadruped IK", "Insect IK"};
	int selectedType = settings.type;
	if (ImGui::Combo("IK Type", &selectedType, types, IM_ARRAYSIZE(types)))
	{
		settings.type = selectedType;
		model->ResetVmdlIKLegsForType();
		MarkDirty();
	}
	if (settings.type == 0) return;

	const auto nodeCombo = [&](const char* label, std::string& name, bool allowNone = false)
	{
		bool changed = false;
		const int selectedNodeIndex = name.empty() ? -1 : model->GetNodeIndex(name.c_str());
		const std::string preview = selectedNodeIndex >= 0
			? MakeNodeLabel(selectedNodeIndex, model->GetNodes()[selectedNodeIndex].name)
			: name.empty() ? "(none)" : name;
		if (ImGui::BeginCombo(label, preview.c_str()))
		{
			if (allowNone && ImGui::Selectable("(none)", name.empty()))
			{
				name.clear();
				changed = true;
			}
			const auto& nodes = model->GetNodes();
			for (int nodeIndex = 0; nodeIndex < static_cast<int>(nodes.size()); ++nodeIndex)
			{
				const auto& node = nodes[nodeIndex];
				const std::string nodeLabel = MakeNodeLabel(nodeIndex, node.name);
				if (ImGui::Selectable(nodeLabel.c_str(), nodeIndex == selectedNodeIndex))
				{
					name = nodeLabel;
					changed = true;
				}
			}
			ImGui::EndCombo();
		}
		if (!name.empty() && model->GetNodeIndex(name.c_str()) < 0)
		{
			ImGui::SameLine();
			ImGui::TextUnformatted("Missing");
		}
		return changed;
	};

	if (nodeCombo(settings.type == 1 ? "Pelvis" : "Body Center", settings.centerNode)) MarkDirty();
	for (int i = 0; i < static_cast<int>(settings.legs.size()); ++i)
	{
		auto& leg = settings.legs[i];
		ImGui::PushID(i);
		const std::string header = leg.name.empty() ? "Leg " + std::to_string(i + 1) : leg.name;
		if (ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
		{
			bool changed = ImGui::InputText("Name", &leg.name);
			changed |= nodeCombo("Root", leg.root);
			changed |= nodeCombo("Mid", leg.mid);
			changed |= nodeCombo("Tip", leg.tip);
			changed |= nodeCombo("Contact", leg.contact, true);
			if (changed) MarkDirty();
		}
		ImGui::PopID();
	}
}

void VmdlEditorScene::DrawMorphEditor()
{
	ImGui::TextUnformatted("Morph Editor");
	ImGui::SameLine();
	ImGui::TextDisabled("+ shows a mesh, - hides it, and square leaves it unchanged.");
	if (!model) return;

	auto& morphs = model->GetVmdlExtensionData().morphs;
	if (ImGui::Button("Register Current Morph"))
	{
		auto& morph = morphs.emplace_back();
		morph.name = MakeUniqueMorphName(*model, "MORPH " + std::to_string(morphs.size()));
		morph.meshVisibility.reserve(model->GetMeshes().size());
		for (const VMDLModel::Mesh& mesh : model->GetMeshes()) morph.meshVisibility.push_back(mesh.isDraw ? 1 : 0);
		selectedMorph = static_cast<int>(morphs.size()) - 1;
		MarkDirty();
	}
	ImGui::SameLine();
	if (ImGui::Button("Apply Morph") && selectedMorph >= 0 && selectedMorph < static_cast<int>(morphs.size()))
	{
		model->ApplyMorph(selectedMorph);
	}
	ImGui::SameLine();
	const bool canDeleteMorph = selectedMorph >= 0 && selectedMorph < static_cast<int>(morphs.size());
	ImGui::BeginDisabled(!canDeleteMorph);
	if (ImGui::Button("Duplicate Morph") && canDeleteMorph)
	{
		VMDLModel::VmdlMorph duplicate = morphs[selectedMorph];
		duplicate.name = MakeUniqueMorphName(*model, duplicate.name + " COPY");
		morphs.push_back(std::move(duplicate));
		selectedMorph = static_cast<int>(morphs.size()) - 1;
		MarkDirty();
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(!canDeleteMorph);
	if (ImGui::Button("Delete Morph") && canDeleteMorph)
	{
		morphs.erase(morphs.begin() + selectedMorph);
		for (auto& track : model->GetVmdlAnimationControlData().morphTracks)
		{
			std::erase_if(track.keys, [&](const auto& key) { return key.morphIndex == selectedMorph; });
			for (auto& key : track.keys)
			{
				if (key.morphIndex > selectedMorph) --key.morphIndex;
			}
		}
		if (morphs.empty()) selectedMorph = -1;
		else selectedMorph = std::min(selectedMorph, static_cast<int>(morphs.size()) - 1);
		MarkDirty();
	}
	ImGui::EndDisabled();

	constexpr ImGuiTableFlags morphTableFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp;
	if (ImGui::BeginTable("Morph Editor Layout", 2, morphTableFlags, ImVec2(0.0f, 0.0f)))
	{
		ImGui::TableSetupColumn("Morph List Column", ImGuiTableColumnFlags_WidthStretch, 0.25f);
		ImGui::TableSetupColumn("Morph Property Column", ImGuiTableColumnFlags_WidthStretch, 0.75f);
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::BeginChild("Morph List", ImVec2(0.0f, 0.0f), true);
		for (int i = 0; i < static_cast<int>(morphs.size()); ++i)
		{
			if (ImGui::Selectable(morphs[i].name.c_str(), selectedMorph == i)) selectedMorph = i;
		}
		ImGui::EndChild();
		ImGui::TableSetColumnIndex(1);
		ImGui::BeginChild("Morph Property", ImVec2(0.0f, 0.0f), true);
		if (selectedMorph >= 0 && selectedMorph < static_cast<int>(morphs.size()))
		{
			auto& morph = morphs[selectedMorph];
			char name[128]{};
			strcpy_s(name, morph.name.c_str());
			if (ImGui::InputText("Name", name, sizeof(name)))
			{
				const std::string newName = ToUpperMorphName(name);
				const int duplicateIndex = model->GetMorphIndex(newName.c_str());
				if (!newName.empty() && (duplicateIndex < 0 || duplicateIndex == selectedMorph))
				{
					morph.name = newName;
					MarkDirty();
				}
			}
			if (morph.meshVisibility.size() != model->GetMeshes().size()) morph.meshVisibility.resize(model->GetMeshes().size(), 2);
			ImGui::TextUnformatted("+ Add");
			ImGui::SameLine();
			ImGui::TextUnformatted("- Remove");
			ImGui::SameLine();
			ImGui::TextDisabled("Middle dot: No change");
			ImGui::Separator();
			for (int i = 0; i < static_cast<int>(model->GetMeshes().size()); ++i)
			{
				uint8_t& state = morph.meshVisibility[i];
				if (state > 2) state = 2;
				const VMDLModel::Mesh& mesh = model->GetMeshes()[i];
				const std::string label = "Mesh " + std::to_string(i) + " : " + mesh.material->name;
				ImGui::PushID(i);
				bool changed = false;
				if (ImGui::RadioButton("+", state == 1)) { state = 1; changed = true; }
				ImGui::SameLine();
				if (ImGui::RadioButton("-", state == 0)) { state = 0; changed = true; }
				ImGui::SameLine();
				if (ImGui::RadioButton("\xC2\xB7", state == 2)) { state = 2; changed = true; }
				ImGui::SameLine();
				ImGui::TextUnformatted(label.c_str());
				if (changed) MarkDirty();
				ImGui::PopID();
			}
		}
		ImGui::EndChild();
		ImGui::EndTable();
	}
}

void VmdlEditorScene::DrawMaterialEditor()
{
	ImGui::TextUnformatted("Material Editor");
	ImGui::SameLine();
	ImGui::TextDisabled("PBR settings are saved in the VMDL.");
	if (!model) return;

	auto& materials = model->GetMaterials();
	if (materials.empty())
	{
		ImGui::TextDisabled("This model has no materials.");
		return;
	}
	selectedMaterial = std::clamp(selectedMaterial, 0, static_cast<int>(materials.size()) - 1);

	constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp;
	if (!ImGui::BeginTable("Material Editor Layout", 2, tableFlags, ImVec2(0.0f, 0.0f))) return;
	ImGui::TableSetupColumn("Material List Column", ImGuiTableColumnFlags_WidthStretch, 0.28f);
	ImGui::TableSetupColumn("Material Property Column", ImGuiTableColumnFlags_WidthStretch, 0.72f);
	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::BeginChild("Material List", ImVec2(0.0f, 0.0f), true);
	for (int i = 0; i < static_cast<int>(materials.size()); ++i)
	{
		ImGui::PushID(i);
		if (ImGui::Selectable(materials[i].name.c_str(), selectedMaterial == i)) selectedMaterial = i;
		ImGui::PopID();
	}
	ImGui::EndChild();

	ImGui::TableSetColumnIndex(1);
	ImGui::BeginChild("Material Property", ImVec2(0.0f, 0.0f), true);
	auto& material = materials[selectedMaterial];
	ImGui::Text("Material: %s", material.name.c_str());
	ImGui::SeparatorText("PBR");
	bool changed = ImGui::ColorEdit4("Base Color", &material.baseColor.x);
	changed |= ImGui::ColorEdit4("Emissive Color", &material.emissiveColor.x, ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
	changed |= ImGui::SliderFloat("Metalness", &material.metalness, 0.0f, 1.0f);
	changed |= ImGui::SliderFloat("Roughness", &material.roughness, 0.0001f, 1.0f);
	changed |= ImGui::SliderFloat("Occlusion", &material.occlusion, 0.0f, 1.0f);
	changed |= ImGui::SliderFloat("Occlusion Strength", &material.occlusionStrength, 0.0f, 1.0f);
	changed |= ImGui::SliderFloat("Shadow Strength", &material.shadowStrength, 0.0f, 1.0f);

	const char* alphaModes[] = {"Opaque", "Mask", "Blend"};
	int alphaMode = static_cast<int>(material.alphaMode);
	if (ImGui::Combo("Alpha Mode", &alphaMode, alphaModes, IM_ARRAYSIZE(alphaModes)))
	{
		material.alphaMode = static_cast<VMDLModel::AlphaMode>(alphaMode);
		changed = true;
	}
	if (material.alphaMode == VMDLModel::AlphaMode::Mask)
		changed |= ImGui::SliderFloat("Alpha Cutoff", &material.alphaCutoff, 0.0f, 1.0f);

	ImGui::SeparatorText("Textures");
	const auto textureRow = [&](const char* label, VMDLModel::MaterialTextureSlot slot,
		const std::string& filename, const std::vector<uint8_t>& embedded)
	{
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::TextUnformatted(label);
		ImGui::TableSetColumnIndex(1);

		if (filename.empty()) ImGui::TextDisabled(embedded.empty() ? "None" : "Embedded");
		else ImGui::Text("%s%s", filename.c_str(), embedded.empty() ? "" : " (Embedded)");

		ImGui::TableSetColumnIndex(2);
		ImGui::PushID(label);

		if (ImGui::SmallButton("Export"))
		{
			char filepath[MAX_PATH]{};
			const char* filter =
				"DDS Texture (*.dds)\0"
				"*.dds\0"
				"PNG Image (*.png)\0"
				"*.png\0"
				"\0";
			if (Dialog::SaveFileName(filepath, MAX_PATH, filter, "Export Material Texture") == DialogResult::OK)
			{
				if (model->ExportMaterialTexture(static_cast<size_t>(selectedMaterial), slot, filepath))
				{
					changed = true;
				}
				else
				{
					ErrorMessage("Failed to export the material texture.");
				}
			}
		}
		ImGui::SameLine();

		if (ImGui::SmallButton("Replace"))
		{
			char filepath[MAX_PATH]{};
			const char* filter =
				"DDS Texture (*.dds)\0"
				"*.dds\0"
				"PNG Image (*.png)\0"
				"*.png\0"
				"\0";
			if (Dialog::OpenFileName(filepath, MAX_PATH, filter, "Replace Material Texture") == DialogResult::OK)
			{
				if (model->ReplaceMaterialTexture(static_cast<size_t>(selectedMaterial), slot, filepath))
				{
					changed = true;
				}
				else
				{
					ErrorMessage("Failed to replace the material texture.");
				}
			}
		}
		ImGui::SameLine();

		if (ImGui::SmallButton("Clear") &&
			model->ClearMaterialTexture(static_cast<size_t>(selectedMaterial), slot))
		{
			changed = true;
		}
		ImGui::PopID();
	};
	if (ImGui::BeginTable("Material Textures", 3, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp))
	{
		ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 150.0f);
		ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 300.0f);
		textureRow("Base Color", VMDLModel::MaterialTextureSlot::BaseColor,
			material.baseTextureFileName, material.baseTextureDDS);
		textureRow("Normal", VMDLModel::MaterialTextureSlot::Normal,
			material.normalTextureFileName, material.normalTextureDDS);
		textureRow("Metalness / Roughness", VMDLModel::MaterialTextureSlot::MetalnessRoughness,
			material.metalnessRoughnessTextureFileName, material.metalnessRoughnessTextureDDS);
		textureRow("Occlusion", VMDLModel::MaterialTextureSlot::Occlusion,
			material.occlusionTextureFileName, material.occlusionTextureDDS);
		textureRow("Emissive", VMDLModel::MaterialTextureSlot::Emissive,
			material.emissiveTextureFileName, material.emissiveTextureDDS);
		ImGui::EndTable();
	}
	if (changed) MarkDirty();
	ImGui::EndChild();
	ImGui::EndTable();
}

void VmdlEditorScene::AddAnimationKey()
{
	RecordSelectedNodeKey();
}

void VmdlEditorScene::RecordSelectedNodeKey()
{
	if (!model || selectedAnimation < 0 || selectedNode < 0) return;
	auto& animation = model->GetAnimations()[selectedAnimation];
	if (animation.nodeAnims.size() < model->GetNodes().size()) animation.nodeAnims.resize(model->GetNodes().size());
	const VMDLModel::Node& node = model->GetNodes()[selectedNode];
	auto& nodeAnimation = animation.nodeAnims[selectedNode];
	const auto upsert = [&](auto& keyframes, const auto& value)
	{
		for (auto& key : keyframes)
		{
			if (std::abs(key.seconds - animationTime) > 0.0005f) continue;
			key.value = value;
			return;
		}
		keyframes.push_back({animationTime, value});
	};
	upsert(nodeAnimation.positionKeyframes, node.position);
	upsert(nodeAnimation.rotationKeyframes, node.rotation);
	upsert(nodeAnimation.scaleKeyframes, node.scale);
	auto byTime = [](const auto& left, const auto& right) { return left.seconds < right.seconds; };
	std::sort(nodeAnimation.positionKeyframes.begin(), nodeAnimation.positionKeyframes.end(), byTime);
	std::sort(nodeAnimation.rotationKeyframes.begin(), nodeAnimation.rotationKeyframes.end(), byTime);
	std::sort(nodeAnimation.scaleKeyframes.begin(), nodeAnimation.scaleKeyframes.end(), byTime);
	MarkDirty();
}

void VmdlEditorScene::MarkDirty()
{
	dirty = true;
	if (!model) return;
	//model->GetVmdlExtensionData().rootOffset = rootOffset;
}


bool VmdlEditorScene::OnRequestExit()
{
	if (dirty)
	{
		int result = MessageBoxW(
			Game::Graphics::Instance().GetWindowHandle(),
			L"終了する前に保存しますか？",
			L"VSTG Editor",
			MB_YESNOCANCEL | MB_ICONQUESTION);
		if (result == IDYES)
		{
			SaveVmdl();
			if (dirty) return false;
		}
		else if (result == IDCANCEL)
		{
			return false; // Cancel exit
		}
	}
	return true;
}

void VmdlEditorScene::UpdateModelFraming()
{
	if (!model) return;

	model->UpdateTransform(Matrix::Identity);
	const Matrix renderScaleTransform = model->GetRenderScaleTransform();

	Vector3 minPosition(FLT_MAX, FLT_MAX, FLT_MAX);
	Vector3 maxPosition(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	bool hasVertex = false;

	for (const VMDLModel::Mesh& mesh : model->GetMeshes())
	{
		if (!mesh.node) continue;

		for (const VMDLModel::Vertex& vertex : mesh.vertices)
		{
			const Vector3 position =
				Vector3::Transform(
					vertex.position,
					mesh.node->globalTransform * renderScaleTransform);

			minPosition.x = std::min(minPosition.x, position.x);
			minPosition.y = std::min(minPosition.y, position.y);
			minPosition.z = std::min(minPosition.z, position.z);

			maxPosition.x = std::max(maxPosition.x, position.x);
			maxPosition.y = std::max(maxPosition.y, position.y);
			maxPosition.z = std::max(maxPosition.z, position.z);

			hasVertex = true;
		}
	}

	if (!hasVertex) return;

	const Vector3 center = (minPosition + maxPosition) * 0.5f;
	const Vector3 size = maxPosition - minPosition;

	cameraFocusOffset =
		center - Vector3(0.0f, 1.0f, 0.0f);

	cameraDistance =
		std::clamp(size.Length() * 1.25f, 2.0f, 50.0f);
	targetCameraDistance = cameraDistance;
}

void VmdlEditorScene::UpdateWindowTitle()
{
	SetWindowTextW(Game::Graphics::Instance().GetWindowHandle(), L"VMDL Editor");
}

void VmdlEditorScene::OpenVmdl()
{
	const std::string initialDirectory =
		(ResourceManager::FindSourceDataRoot() / "Model").string();
	char filepath[MAX_PATH]{};
	if (Dialog::OpenFileName(
		filepath,
		MAX_PATH,
		"VMDL (*.vmdl)\0*.vmdl\0",
		"Open VMDL",
		initialDirectory.c_str()
		) != DialogResult::OK)
		return;
	LoadModel(filepath, false);
}

void VmdlEditorScene::ImportGlb()
{
	const std::string initialDirectory =
		(ResourceManager::FindSourceDataRoot() / "Model").string();
	char filepath[MAX_PATH]{};
	if (Dialog::OpenFileName(
		filepath,
		MAX_PATH,
		"glTF Binary (*.glb)\0*.glb\0",
		"Import GLB",
		initialDirectory.c_str()
		) != DialogResult::OK)
		return;
	LoadModel(filepath, true);
}

void VmdlEditorScene::SaveVmdl()
{
	if (!model) return;
	if (documentPath.empty())
	{
		SaveVmdlAs();
		return;
	}
	if (model->SaveVmdl(documentPath))
	{
		displayPath = documentPath;
		dirty = false;
	}
	else
	{
		ErrorMessage("Failed to save the VMDL file.");
	}
}

void VmdlEditorScene::SaveVmdlAs()
{
	if (!model) return;
	char filepath[MAX_PATH]{};
	if (!documentPath.empty()) strcpy_s(filepath, documentPath.string().c_str());
	if (Dialog::SaveFileName(filepath, MAX_PATH, "VMDL (*.vmdl)\0*.vmdl\0", "Save VMDL", "vmdl") != DialogResult::OK) return;
	documentPath = filepath;
	if (model->SaveVmdl(documentPath))
	{
		displayPath = documentPath;
		dirty = false;
	}
	else
	{
		ErrorMessage("Failed to save the VMDL file.");
	}
}

void VmdlEditorScene::LoadModel(const std::filesystem::path& filepath, bool importGlb)
{
	try
	{
		if (importGlb)
		{
			std::filesystem::path proposedPath = filepath;
			proposedPath.replace_extension(".vmdl");
			model = std::make_shared<VMDLModel>(filepath.string().c_str(), 60.0f, true, proposedPath.string().c_str(), false);
			documentPath.clear();
			displayPath = filepath;
			dirty = true;
		}
		else
		{
			documentPath = filepath;
			displayPath = filepath;
			model = std::make_shared<VMDLModel>(filepath.string().c_str());
			dirty = false;
		}
		selectedNode = model->GetNodes().empty() ? -1 : 0;
		selectedMesh = -1;
		selectedMaterial = model->GetMaterials().empty() ? -1 : 0;
		selectedAnimation = model->GetAnimations().empty() ? -1 : 0;
		animationTime = 0.0f;
		animationPlaying = false;
		selectedKeyTrack = -1;
		selectedKeyIndex = -1;
		UpdateModelFraming();
		ResetAnimationControlPreview();
	}
	catch (const std::exception& exception)
	{
		model.reset();
		displayPath.clear();
		ErrorMessage(std::string("Load failed: ") + exception.what());
	}

	model->ApplyMorph("DEFAULT");
}

void VmdlEditorScene::ErrorMessage(const std::string& message)
{
	MessageBoxW(
		Game::Graphics::Instance().GetWindowHandle(),
		std::wstring(message.begin(), message.end()).c_str(), L"VMDL Editor", MB_ICONERROR);
}
