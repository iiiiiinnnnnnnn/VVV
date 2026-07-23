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
	CreateStage();
}

void VstgEditorScene::CreateStage()
{
	currentStage = std::make_unique<Stage>();
	auto* rigidbody = currentStage->AddComponent<RigidbodyStatic>();
	terrain = currentStage->AddComponent<Terrain>();
	currentStage->AddComponent<TerrainMeshCollider>(Layers::Get("Terrain"), rigidbody);
	stageLoader = currentStage->AddComponent<StageLoader>(currentStage.get(), std::string("{}"), true);
	Camera* camera = currentStage->GetActiveCamera();
	camera->SetPerspectiveFov(RAD(45.0f), Game::Graphics::ScreenWidth / Game::Graphics::ScreenHeight, 0.1f, 2000.0f);
	camera->SetLookAt({0.0f, 20.0f, -30.0f}, Vector3::Zero, Vector3::Up);
	currentStage->GetDefaultCameraActor()->AddComponent<FreeCameraController>();
}

void VstgEditorScene::OnDrawGUI()
{
	UpdateTitle();
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	constexpr ImGuiWindowFlags flags =
		ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
	if (!ImGui::Begin("VSTG Editor", nullptr, flags))
	{
		ImGui::End();
		return;
	}
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Open VSTG")) Open();
			if (ImGui::MenuItem("Save VSTG", "Ctrl+S")) Save();
			if (ImGui::MenuItem("Save VSTG As", "Ctrl+Shift+S")) SaveAs();
			if (ImGui::MenuItem("Import DDS + Stage JSON")) ImportLegacy();
			if (ImGui::MenuItem("Exit")) SceneManager::Instance().LoadScene<GameStartScene>();
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}
	if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
	{
		if (ImGui::GetIO().KeyShift) SaveAs();
		else Save();
	}
	if (ImGui::BeginTable("VSTG Layout", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
	{
		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Lighting");
		if (ImGui::Button("Add Point Light")) currentStage->GetLightManager().AddPointLight();
		ImGui::SameLine();
		if (ImGui::Button("Add Spot Light")) currentStage->GetLightManager().AddSpotLight();
		if (ImGui::Button("Add Area Light")) currentStage->GetLightManager().AddAreaLight();
		currentStage->GetLightManager().DrawGUI();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Terrain");
		if (terrain) terrain->DrawGUI();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Props / Spawners");
		if (stageLoader) stageLoader->DrawGUI();
		ImGui::EndTable();
	}
	if (!message.empty()) ImGui::TextWrapped("%s", message.c_str());
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
	message = "VSTG loaded.";
}

void VstgEditorScene::Save()
{
	if (path.empty())
	{
		SaveAs();
		return;
	}
	if (!data.Capture(*terrain, *stageLoader, currentStage->GetLightManager()) || !data.Save(path))
	{
		message = data.GetError();
		return;
	}
	ResourceManager::Instance().RegisterGeneratedCache(path.generic_string());
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

void VstgEditorScene::ImportLegacy()
{
	char terrainPath[MAX_PATH]{};
	if (Dialog::OpenFileName(terrainPath, MAX_PATH, "Terrain DDS (*.dds)\0*.dds\0", "Import Terrain DDS") != DialogResult::OK) return;
	char jsonPath[MAX_PATH]{};
	if (Dialog::OpenFileName(jsonPath, MAX_PATH, "Stage JSON (*.json)\0*.json\0", "Import Stage JSON") != DialogResult::OK) return;
	std::ifstream stream(jsonPath);
	if (!stream)
	{
		message = "Stage JSON could not be opened.";
		return;
	}
	const std::string text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
	CreateStage();
	if (!terrain->LoadTerrainTexture(terrainPath))
	{
		message = "Terrain DDS import failed.";
		return;
	}
	stageLoader->LoadJsonText(text);
	path.clear();
	message = "Legacy DDS and Stage JSON imported. Save as VSTG.";
}

void VstgEditorScene::UpdateTitle()
{
	std::wstring title = L"VSTG Editor - ";
	title += path.empty() ? L"Untitled" : path.wstring();
	SetWindowTextW(Game::Graphics::Instance().GetWindowHandle(), title.c_str());
}
