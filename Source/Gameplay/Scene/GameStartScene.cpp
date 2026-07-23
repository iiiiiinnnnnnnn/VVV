// GameStartScene.cpp

#include "Gameplay/Scene/GameStartScene.h"

#include "Gameplay/Scene/SceneManager.h"
#include "Gameplay/Scene/TestPlayScene.h"
#include "Gameplay/Scene/VmdlEditorScene.h"
#include "Resource/ResourceManager.h"

GameStartScene::GameStartScene(SceneMessage message)
	: Scene(message)
{
	resourcesReady = ResourceManager::Instance().PrepareGameResources();
}

void GameStartScene::OnUpdate()
{
	if (!resourcesReady || loadRequested) return;
#if !defined(_DEBUG)
	loadRequested = SceneManager::Instance().LoadScene<TestPlayScene>();
#endif
}

void GameStartScene::OnDrawGUI()
{
	if (resourcesReady)
	{
		ImGui::TextUnformatted("Select startup mode.");
#if defined(_DEBUG)
		if (ImGui::Button("Enter Game", ImVec2(240, 56)))
		{
			loadRequested = SceneManager::Instance().LoadScene<TestPlayScene>();
		}
		if (ImGui::Button("Enter VMDL Editor", ImVec2(240, 56)))
		{
			loadRequested = SceneManager::Instance().LoadScene<VmdlEditorScene>();
		}
#endif
		return;
	}

	ImGui::TextUnformatted("Resource preparation failed.");
	for (const std::string& error : ResourceManager::Instance().GetErrors())
	{
		ImGui::TextWrapped("%s", error.c_str());
	}
}
