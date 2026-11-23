#include "Application/SettingsAndDebug/PhysicsLayerManager.h"
#include <imgui.h>
#include <imgui_stdlib.h>
#include "Physics/Core/PhysicsManager.h"
#include <fstream>
#include <stdexcept>
#include <filesystem>
#include "Application/Tools/Dialog.h"
#include "Resource/ResourceManager.h"
#include "Resource/ResourcePath.h"

// Cereal
#include <cereal/cereal.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/utility.hpp>
#include <cereal/types/array.hpp>


std::filesystem::path GetPhysicsLayerPath()
{
	return ResourceManager::Instance().ResolveEditPath("PhysicsLayers", ResourceManager::ResourceType::PhysicsLayers);
}

template<class Archive>
void PhysicsLayerManager::PhysicsLayers::serialize(Archive& archive)
{
	archive(
		CEREAL_NVP(layerNames),
		CEREAL_NVP(collisionMatrix)
	);
}

bool PhysicsLayerManager::Load(const std::string& path)
{
	try
	{
		const auto filepath = ResourceManager::Instance().ResolveEditPath(path, ResourceManager::ResourceType::PhysicsLayers);
		std::ifstream ifs(filepath, std::ios::binary);
		if (!ifs) throw std::runtime_error("Cannot open physics layers");
		PhysicsLayers loaded{};
		cereal::BinaryInputArchive archive(ifs);
		archive(loaded);
		settings = std::move(loaded);
		currentPath = filepath.string();
		pathInput = currentPath;
		fileStatus = "読み込みました。";
		PhysicsManager::Instance().RefreshLayerFiltering();
		return true;
	}
	catch (const std::exception&)
	{
		fileStatus = "読み込みに失敗しました。";
		return false;
	}
}

bool PhysicsLayerManager::Save(const std::string& path)
{
	try
	{
		const auto filepath = ResourceManager::Instance().ResolveEditPath(path, ResourceManager::ResourceType::PhysicsLayers);
		const auto saved = ResourceManager::Instance().SaveResourceFile(filepath, [&](const std::filesystem::path& destination) {
			std::ofstream ofs(destination, std::ios::binary);
			if (!ofs) throw std::runtime_error("Cannot save physics layers");
			{
				cereal::BinaryOutputArchive archive(ofs);
				archive(CEREAL_NVP(settings));
			}
			ofs.close();
			if (!ofs) throw std::runtime_error("Cannot write physics layers");
		});
		currentPath = saved.string();
		pathInput = currentPath;
		fileStatus = "保存しました。";
		return true;
	}
	catch (const std::exception&)
	{
		fileStatus = "保存に失敗しました。";
		return false;
	}
}

void PhysicsLayerManager::CreateDefault()
{
	settings = {};
	settings.layerNames[0] = "Default";
	settings.collisionMatrix[0][0] = 1;
	PhysicsManager::Instance().RefreshLayerFiltering();
}

bool PhysicsLayerManager::Collides(LayerId a, LayerId b) const
{
	if (a == InvalidLayerId || b == InvalidLayerId)
		return false;

	if (a == NothingLayerId || b == NothingLayerId)
		return false;

	if (a == EverythingLayerId || b == EverythingLayerId)
		return true;

	if (a >= EditableLayerCount || b >= EditableLayerCount)
		return false;

	if (settings.layerNames[a].empty())
		return false;

	if (settings.layerNames[b].empty())
		return false;

	return settings.collisionMatrix[a][b] != 0;
}

void PhysicsLayerManager::SetCollides(LayerId a, LayerId b, bool enabled)
{
	const size_t ia = static_cast<size_t>(a);
	const size_t ib = static_cast<size_t>(b);

	if (ia >= settings.collisionMatrix.size())
		return;

	if (ib >= settings.collisionMatrix[ia].size())
		return;

	const uint8_t value = enabled ? 1 : 0;

	settings.collisionMatrix[ia][ib] = value;

	if (ib < settings.collisionMatrix.size() &&
		ia < settings.collisionMatrix[ib].size())
	{
		settings.collisionMatrix[ib][ia] = value;
	}

	PhysicsManager::Instance().RefreshLayerFiltering();
}

void PhysicsLayerManager::Initialize()
{
	currentPath = GetPhysicsLayerPath().string();
	pathInput = currentPath;
	if (!Load(currentPath)) CreateDefault();
}

void PhysicsLayerManager::DrawGUI(bool* open)
{
	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.0f);
	ImGui::SetNextWindowBgAlpha(1.0f);
	if (ImGui::Begin(ICON_FA_LAYER_GROUP " 物理レイヤー", open, ImGuiWindowFlags_MenuBar))
	{
		const char* filter = "Physics Layers (*.physicslayers)\0*.physicslayers\0";
		auto openFile = [&]()
		{
			std::string path = pathInput;
			if (Dialog::OpenFileName(path, filter, "物理レイヤーを開く") == DialogResult::OK) Load(path);
		};
		auto saveAs = [&]()
		{
			std::string path = pathInput;
			if (Dialog::SaveFileName(path, filter, "物理レイヤーを保存", "physicslayers") == DialogResult::OK) Save(path);
		};
		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("ファイル"))
			{
				if (ImGui::MenuItem("新規作成"))
				{
					CreateDefault();
					currentPath.clear();
					pathInput.clear();
					fileStatus = "新規作成（未保存）";
				}
				if (ImGui::MenuItem("開く...")) openFile();
				if (ImGui::MenuItem("保存"))
				{
					if (currentPath.empty()) saveAs();
					else Save(currentPath);
				}
				if (ImGui::MenuItem("名前を付けて保存...")) saveAs();
				ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
		}
		ImGui::SetNextItemWidth(-100.0f);
		ImGui::InputText("パス", &pathInput);
		ImGui::SameLine();
		if (ImGui::Button("参照...")) openFile();
		if (ImGui::Button(ICON_FA_SAVE " 保存"))
		{
			if (pathInput.empty()) saveAs();
			else Save(pathInput);
		}
		ImGui::SameLine();
		if (ImGui::Button(ICON_FA_RECYCLE " 読み込み"))
		{
			if (pathInput.empty()) openFile();
			else Load(pathInput);
		}
		if (!fileStatus.empty()) ImGui::Text("%s", fileStatus.c_str());
		ImGui::Separator();

		constexpr int DisplayLayerCount = EditableLayerCount;

		auto getDisplayLayerName = [&](int index) -> std::string
		{
			const int realIndex = index;
			const std::string& name = settings.layerNames[realIndex];

			return std::to_string(index) + ": " + (name.empty() ? ("(空)") : name);
		};

		auto getDisplayLayerId = [&](int index) -> LayerId
		{
			return static_cast<LayerId>(index);
		};

		if (ImGui::BeginTable(
			"CollisionMatrix",
			DisplayLayerCount + 1,
			ImGuiTableFlags_Borders |
			ImGuiTableFlags_RowBg |
			ImGuiTableFlags_ScrollX |
			ImGuiTableFlags_ScrollY |
			ImGuiTableFlags_SizingFixedFit))
		{
			constexpr float LayerColumnWidth = 220.0f;
			constexpr float CollisionColumnWidth = 100.0f;

			ImGui::TableSetupColumn(
				"Layer",
				ImGuiTableColumnFlags_WidthFixed,
				LayerColumnWidth);

			for (int x = 0; x < DisplayLayerCount; ++x)
			{
				ImGui::TableSetupColumn(
					getDisplayLayerName(x).c_str(),
					ImGuiTableColumnFlags_WidthFixed,
					CollisionColumnWidth);
			}

			ImGui::TableSetupScrollFreeze(1, 1);
			ImGui::TableHeadersRow();

			for (int y = 0; y < DisplayLayerCount; ++y)
			{
				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);

				ImGui::PushID(y);
				ImGui::Text("%d:", y);
				ImGui::SameLine();
				ImGui::SetNextItemWidth(175.0f);
				if (ImGui::InputTextWithHint(
						"##LayerName", ("(空)"), &settings.layerNames[y]))
					PhysicsManager::Instance().RefreshLayerFiltering();
				ImGui::PopID();

				for (int x = 0; x < DisplayLayerCount; ++x)
				{
					ImGui::TableSetColumnIndex(x + 1);

					const LayerId layerA = getDisplayLayerId(y);
					const LayerId layerB = getDisplayLayerId(x);

					bool enabled = Collides(layerA, layerB);

					const std::string id = "##Collision_" +
						std::to_string(y) + "_" + std::to_string(x);

					if (ImGui::Checkbox(id.c_str(), &enabled))
					{
						SetCollides(layerA, layerB, enabled);
					}
				}
			}

			ImGui::EndTable();
		}
	}

	ImGui::End();
	ImGui::PopStyleVar();
}

LayerId PhysicsLayerManager::GetLayerId(const char* name) const
{
	if (strcmp(name, "Everything") == 0)
		return EverythingLayerId;

	if (strcmp(name, "Nothing") == 0)
		return NothingLayerId;

	for (int i = 0; i < EditableLayerCount; ++i)
	{
		if (settings.layerNames[i].empty())
			continue;

		if (settings.layerNames[i] == name)
			return static_cast<LayerId>(i);
	}

	return InvalidLayerId;
}

const std::string& PhysicsLayerManager::GetLayerName(LayerId id) const
{
	static const std::string empty;
	if (id >= EditableLayerCount) return empty;
	return settings.layerNames[id];
}

std::string PhysicsLayerManager::GetLayerDisplayName(LayerId id) const
{
	if (id >= EditableLayerCount) return std::to_string(id) + ": (Invalid)";
	const std::string& name = GetLayerName(id);
	return std::to_string(id) + ": " + (name.empty() ? ("(空)") : name);
}
