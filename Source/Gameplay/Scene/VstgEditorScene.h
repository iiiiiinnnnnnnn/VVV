// VstgEditorScene.h

#pragma once

#include <filesystem>
#include <string>

#include "Gameplay/Scene/Scene.h"
#include "Resource/VSTG.h"

class StageLoader;
class Terrain;
class NavMeshActor;

class VstgEditorScene : public Scene
{
public:
	VstgEditorScene(SceneMessage message = nullptr);
	~VstgEditorScene() override;

protected:
	void ConfigureRenderSettings(RenderSettings& settings) override;
	void OnRender(RenderContext& rc) override;
	void OnDrawGUI() override;
	bool UsesGameDebugGUI() const override { return false; }
	bool OnRequestExit() override;

private:
	void CreateStage();
	void Open();
	void Save();
	void SaveAs();
	void UpdateTitle();
	void ErrorMessage(const std::string& message);

	std::filesystem::path path;
	VSTG data;
	Terrain* terrain = nullptr;
	NavMeshActor* navMesh = nullptr;
	StageLoader* stageLoader = nullptr;
	bool showFog = true;
	bool dirty = true;
};
