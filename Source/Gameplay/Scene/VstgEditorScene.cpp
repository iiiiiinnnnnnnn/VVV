// VstgEditorScene.cpp

#include "Gameplay/Scene/VstgEditorScene.h"

#include <fstream>

#include "Application/Tools/Dialog.h"
#include "Gameplay/Camera/Camera.h"
#include "Gameplay/Camera/FreeCameraController.h"
#include "Gameplay/Scene/GameStartScene.h"
#include "Gameplay/Scene/SceneManager.h"
#include "Gameplay/Stage/Component/StageLoader.h"
#include "Gameplay/Stage/Component/Terrain.h"
#include "Gameplay/Stage/Stage.h"
#include "Physics/Collider/TerrainMeshCollider.h"
#include "Physics/RigidBody/Rigidbody.h"
#include "Resource/ResourceManager.h"

VstgEditorScene::VstgEditorScene(SceneMessage message)
	: Scene(message)
{
	Game::Graphics& graphics = Game::Graphics::Instance();
	graphics.SetBorderlessFullscreen(true);
	graphics.SetWindowMovementLocked(true);
	CreateStage();
}

VstgEditorScene::~VstgEditorScene()
{
	Game::Graphics& graphics = Game::Graphics::Instance();
	graphics.SetBorderlessFullscreen(false);
	graphics.SetWindowMovementLocked(false);
}

void VstgEditorScene::CreateStage()
{
	currentStage = std::make_unique<Stage>();
	DirectionalLight& directionalLight = currentStage->GetLightManager().GetDirectionalLight();
	directionalLight.transform.rotation = Quaternion::CreateFromYawPitchRoll(
		RAD(-35.0f),
		RAD(35.0f),
		0.0f);
	directionalLight.transform.Update();
	currentStage->GetLightManager().SetAmbientColor(ColorFromRGBA(0x2A4C7DFF));
	auto* rigidbody = currentStage->AddComponent<RigidbodyStatic>();
	terrain = currentStage->AddComponent<Terrain>();
	currentStage->AddComponent<TerrainMeshCollider>(
		Layers::Get("Terrain"),
		rigidbody,
		TerrainMeshCollider::CollisionArea{},
		nullptr,
		false);
	stageLoader = currentStage->AddComponent<StageLoader>(currentStage.get(), std::string("{}"), true);
	Camera* camera = currentStage->GetActiveCamera();
	camera->SetPerspectiveFov(RAD(45.0f), Game::Graphics::ScreenWidth / Game::Graphics::ScreenHeight, 0.1f, 2000.0f);
	camera->SetLookAt({0.0f, 20.0f, -30.0f}, Vector3::Zero, Vector3::Up);
	currentStage->GetDefaultCameraActor()->AddComponent<FreeCameraController>();
}

void VstgEditorScene::OnDrawGUI()
{
	UpdateTitle();
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Open VSTG")) Open();
			if (ImGui::MenuItem("Save VSTG", "Ctrl+S")) Save();
			if (ImGui::MenuItem("Save VSTG As", "Ctrl+Shift+S")) SaveAs();
			if (ImGui::MenuItem("Exit")) SceneManager::Instance().LoadScene<GameStartScene>();
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Display"))
		{
			ImGui::Checkbox("Colliders", &renderSettings.showColliderDebug);
			ImGui::Checkbox("Components", &renderSettings.showComponentDebug);
			ImGui::Checkbox("Wireframe", &renderSettings.wireframe);
			ImGui::EndMenu();
		}
		std::string displayPath = path.empty() ? "Untitled" : path.string();
		if (dirty) displayPath += " *";
		const float pathWidth = ImGui::CalcTextSize(displayPath.c_str()).x;
		ImGui::SetCursorPosX(std::max(
			ImGui::GetCursorPosX() + 20.0f,
			ImGui::GetWindowWidth() - pathWidth - ImGui::GetStyle().WindowPadding.x));
		ImGui::TextUnformatted(displayPath.c_str());
		ImGui::EndMainMenuBar();
	}

	if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
	{
		if (ImGui::GetIO().KeyShift) SaveAs();
		else Save();
	}

	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	const ImVec2 workPosition = viewport->WorkPos;
	const ImVec2 workSize = viewport->WorkSize;
	const float leftWidth = workSize.x * 0.24f;
	const float rightWidth = workSize.x * 0.24f;
	const float halfHeight = workSize.y * 0.5f;
	constexpr ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;

	ImGui::SetNextWindowPos(workPosition, ImGuiCond_Always);
	ImGui::SetNextWindowSize({leftWidth, halfHeight}, ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Lights###VSTG Lights", nullptr, windowFlags))
	{
		if (ImGui::Button("Add Point Light"))
		{
			currentStage->GetLightManager().AddPointLight();
			dirty = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Add Spot Light"))
		{
			currentStage->GetLightManager().AddSpotLight();
			dirty = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Add Area Light"))
		{
			currentStage->GetLightManager().AddAreaLight();
			dirty = true;
		}
		currentStage->GetLightManager().DrawGUI();
		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
			(ImGui::IsAnyItemActive() || ImGui::IsMouseReleased(ImGuiMouseButton_Left)))
			dirty = true;
	}
	ImGui::End();

	ImGui::SetNextWindowPos({workPosition.x, workPosition.y + halfHeight}, ImGuiCond_Always);
	ImGui::SetNextWindowSize({leftWidth, workSize.y - halfHeight}, ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Terrain###VSTG Terrain", nullptr, windowFlags))
	{
		if (terrain) terrain->DrawGUI();
		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
			(ImGui::IsAnyItemActive() || ImGui::IsMouseReleased(ImGuiMouseButton_Left)))
			dirty = true;
	}
	ImGui::End();

	ImGui::SetNextWindowPos(
		{workPosition.x + workSize.x - rightWidth, workPosition.y},
		ImGuiCond_Always);
	ImGui::SetNextWindowSize({rightWidth, workSize.y * 0.95f}, ImGuiCond_Appearing);
	if (ImGui::Begin("Stage Objects###VSTG Stage Objects", nullptr, windowFlags))
	{
		if (ImGui::BeginTabBar("VSTG Object Tabs"))
		{
			if (ImGui::BeginTabItem("Props"))
			{
				if (stageLoader) stageLoader->DrawPropGUI();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Spawners"))
			{
				if (stageLoader) stageLoader->DrawSpawnerGUI();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Crystals"))
			{
				if (stageLoader) stageLoader->DrawCrystalGUI();
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
			(ImGui::IsAnyItemActive() || ImGui::IsMouseReleased(ImGuiMouseButton_Left)))
			dirty = true;
	}
	ImGui::End();
}

void VstgEditorScene::Open()
{
	char filename[MAX_PATH]{};
	if (Dialog::OpenFileName(filename, MAX_PATH, "VSTG (*.vstg)\0*.vstg\0", "Open VSTG") != DialogResult::OK) return;
	VSTG loaded;
	if (!loaded.Load(filename))
	{
		message = loaded.GetError();
		return;
	}
	CreateStage();
	if (!loaded.Apply(*terrain, *stageLoader, currentStage->GetLightManager()))
	{
		message = "VSTG apply failed.";
		return;
	}
	data = std::move(loaded);
	path = filename;
	dirty = false;
	message = "VSTG loaded.";
}

void VstgEditorScene::Save()
{
	if (path.empty())
	{
		SaveAs();
		return;
	}
	terrain->BakeCollider();
	if (!data.Capture(*terrain, *stageLoader, currentStage->GetLightManager()) || !data.Save(path))
	{
		message = data.GetError();
		return;
	}
	ResourceManager::Instance().RegisterGeneratedCache(path.generic_string());
	dirty = false;
	message = "VSTG saved.";
}

void VstgEditorScene::SaveAs()
{
	char filename[MAX_PATH]{};
	if (Dialog::SaveFileName(filename, MAX_PATH, "VSTG (*.vstg)\0*.vstg\0", "Save VSTG", "vstg") != DialogResult::OK) return;
	path = filename;
	if (path.extension() != ".vstg") path.replace_extension(".vstg");
	Save();
}

void VstgEditorScene::UpdateTitle()
{
	std::wstring title = L"VSTG Editor - ";
	title += path.empty() ? L"Untitled" : path.wstring();
	if (dirty) title += L" *";
	SetWindowTextW(Game::Graphics::Instance().GetWindowHandle(), title.c_str());
}
