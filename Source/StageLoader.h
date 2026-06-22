// StageLoader.h

#pragma once

#include "Common.h"
#include "Component.h"
#include "Actor.h"
#include "nlohmann/json.hpp"

class StageLoader : public Component
{
public:
	StageLoader(Object* owner, Actor* stage, std::filesystem::path jsonPath);
	~StageLoader() = default;

	void Update() override;
	void DrawGUI() override;

	void LoadJson();
	void SaveJson();

private:
	std::filesystem::path jsonPath;
	Actor* stage;
};
