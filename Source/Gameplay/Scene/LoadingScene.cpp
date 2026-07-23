// LoadingScene.cpp

#include "Gameplay/Scene/LoadingScene.h"
#include "Gameplay/Scene/SceneManager.h"

LoadingScene::LoadingScene(SceneMessage message) : Scene(message)
{
}

void LoadingScene::OnDrawGUI()
{
	const float progress =
		SceneManager::Instance().GetLoadProgress();

	const std::string error =
		SceneManager::Instance().GetLastLoadError();

	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	constexpr float padding = 10.0f;
	ImGui::SetNextWindowPos(
		{ viewport->WorkPos.x + padding, viewport->WorkPos.y + padding },
		ImGuiCond_Always,
		{ 0.0f, 0.0f });
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGui::SetNextWindowBgAlpha(0.35f);

	constexpr ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoNav |
		ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_NoDocking;

	if (!ImGui::Begin("LoadingOverlay", nullptr, flags))
	{
		ImGui::End();
		return;
	}

	const int percentage =
		static_cast<int>(progress * 100.0f + 0.5f);

	ImGui::Text("Loading... %d%%", percentage);
	ImGui::ProgressBar(
		progress,
		{ 180.0f, 8.0f },
		"");

	if (!error.empty())
	{
		ImGui::Spacing();
		ImGui::TextWrapped("Load Error: %s", error.c_str());
	}

	ImGui::End();
}
