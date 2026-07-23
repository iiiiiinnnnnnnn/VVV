// VstgEditorScene.h

#pragma once

#include <filesystem>

#include "Gameplay/Scene/Scene.h"
#include "Resource/VSTG.h"

class StageLoader;
class Terrain;

class VstgEditorScene : public Scene
{
public:
	VstgEditorScene(SceneMessage message = nullptr);

protected:
	void OnDrawGUI() override;

private:
	void CreateStage();
	void Open();
	void Save();
	void SaveAs();
	void ImportLegacy();
	void UpdateTitle();

	std::filesystem::path path;
	VSTG data;
	Terrain* terrain = nullptr;
	StageLoader* stageLoader = nullptr;
	std::string message;
};
