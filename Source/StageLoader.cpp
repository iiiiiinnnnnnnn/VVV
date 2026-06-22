// StageLoader.cpp

#include "StageLoader.h"
#include "IconsFontAwesome5.h"

using json = nlohmann::json;

StageLoader::StageLoader(Object* owner, Actor* stage, std::filesystem::path jsonPath)
	: Component(owner), stage(stage), jsonPath(jsonPath)
{
	LoadJson();
}

void StageLoader::Update()
{
}

void StageLoader::DrawGUI()
{
	if(ImGui::TreeNode(ICON_FA_BOX "StageLoader"))
	{
		ImGui::Text("Json Path: %s", jsonPath.string().c_str());
		if (ImGui::Button("Load")) LoadJson();
		ImGui::SameLine();
		if (ImGui::Button("Save")) SaveJson();


		ImGui::TreePop();
	}
}

void StageLoader::LoadJson()
{

}

void StageLoader::SaveJson()
{

}
