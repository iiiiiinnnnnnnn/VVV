// VstgEditorScene.h

#pragma once

#include <filesystem>
#include <string>

#include "Gameplay/Scene/Scene.h"
#include "Resource/VSTG.h"

class StageLoader;
class Terrain;

class VstgEditorScene : public Scene
{
public:
	VstgEditorScene(SceneMessage message = nullptr);
	~VstgEditorScene() override;

protected:
	void OnDrawGUI() override;
	bool UsesGameDebugGUI() const override { return false; }

private:
	void CreateStage();
	void Open();
	void Save();
	void SaveAs();
	void UpdateTitle();

	std::filesystem::path path;
	VSTG data;
	Terrain* terrain = nullptr;
	StageLoader* stageLoader = nullptr;
	std::string message;
	bool dirty = true;
};
