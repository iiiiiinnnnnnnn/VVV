// GameStartScene.cpp

#include "Gameplay/Scene/GameStartScene.h"

#include "Gameplay/Scene/SceneManager.h"
#include "Gameplay/Scene/TestPlayScene.h"
#include "Resource/ResourceManager.h"

GameStartScene::GameStartScene(SceneMessage message)
	: Scene(message)
{
	resourcesReady = ResourceManager::Instance().PrepareGameResources();
}

void GameStartScene::OnUpdate()
{
	if (!resourcesReady || loadRequested) return;
	loadRequested = SceneManager::Instance().LoadScene<TestPlayScene>();
}

void GameStartScene::OnDrawGUI()
{
	if (resourcesReady)
	{
		ImGui::TextUnformatted("Resources are ready.");
		return;
	}

	ImGui::TextUnformatted("Resource preparation failed.");
	for (const std::string& error : ResourceManager::Instance().GetErrors())
	{
		ImGui::TextWrapped("%s", error.c_str());
	}
}
