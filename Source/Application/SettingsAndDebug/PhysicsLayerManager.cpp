#include "Application/SettingsAndDebug/PhysicsLayerManager.h"
#include <ImGuiToolKit/Widgets.h>
#include "Physics/Core/PhysicsManager.h"
#include <fstream>
#include <stdexcept>
#include <filesystem>
#include "Application/Tools/Dialog.h"

// Cereal
#include <cereal/cereal.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/utility.hpp>
#include <cereal/types/array.hpp>

#include "imgui.h"
#include "imgui_stdlib.h"

std::filesystem::path GetPhysicsLayerPath()
{
	const std::filesystem::path runtimePath =
		std::filesystem::current_path() / "Resources/PhysicsLayers.physicslayers";
	if (std::filesystem::exists(runtimePath)) return runtimePath;

	for (std::filesystem::path directory = std::filesystem::current_path();
		!directory.empty(); directory = directory.parent_path())
	{
		if (std::filesystem::exists(directory / "VEER.sln"))
			return directory / "Resources/PhysicsLayers.physicslayers";
		if (directory == directory.root_path()) break;
	}
	return runtimePath;
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
		const auto filepath = std::filesystem::absolute(std::filesystem::path(path));
		std::ifstream ifs(filepath, std::ios::binary);
		if (!ifs) throw std::runtime_error("Cannot open physics layers");
		PhysicsLayers loaded{};
		cereal::BinaryInputArchive archive(ifs);
		archive(loaded);
		settings = std::move(loaded);
		currentPath = filepath.string();
		pathInput = currentPath;
		fileStatus = JText(u8"読み込みました。");
		PhysicsManager::Instance().RefreshLayerFiltering();
		return true;
	}
	catch (const std::exception&)
	{
		fileStatus = JText(u8"読み込みに失敗しました。");
		return false;
	}
}

bool PhysicsLayerManager::Save(const std::string& path)
{
	try
	{
		const auto filepath = std::filesystem::absolute(std::filesystem::path(path));
		std::ofstream ofs(filepath, std::ios::binary);
		if (!ofs) throw std::runtime_error("Cannot save physics layers");
		{
			cereal::BinaryOutputArchive archive(ofs);
			archive(CEREAL_NVP(settings));
		}
		ofs.close();
		if (!ofs) throw std::runtime_error("Cannot write physics layers");
		currentPath = filepath.string();
		pathInput = currentPath;
		fileStatus = JText(u8"保存しました。");
		return true;
	}
	catch (const std::exception&)
	{
		fileStatus = JText(u8"保存に失敗しました。");
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
	if (ImGuiToolKit::Begin(ICON_FA_LAYER_GROUP u8" 物理レイヤー", open, ImGuiWindowFlags_MenuBar))
	{
		const char* filter = "Physics Layers (*.physicslayers)\0*.physicslayers\0";
		auto openFile = [&]()
		{
			std::string path = pathInput;
			if (Dialog::OpenFileName(path, filter, JText(u8"物理レイヤーを開く")) == DialogResult::OK) Load(path);
		};
		auto saveAs = [&]()
		{
			std::string path = pathInput;
			if (Dialog::SaveFileName(path, filter, JText(u8"物理レイヤーを保存"), "physicslayers") == DialogResult::OK) Save(path);
		};
		if (ImGui::BeginMenuBar())
		{
			if (ImGuiToolKit::BeginMenu(u8"ファイル"))
			{
				if (ImGuiToolKit::MenuItem(u8"新規作成"))
				{
					CreateDefault();
					currentPath.clear();
					pathInput.clear();
					fileStatus = JText(u8"新規作成（未保存）");
				}
				if (ImGuiToolKit::MenuItem(u8"開く...")) openFile();
				if (ImGuiToolKit::MenuItem(u8"保存"))
				{
					if (currentPath.empty()) saveAs();
					else Save(currentPath);
				}
				if (ImGuiToolKit::MenuItem(u8"名前を付けて保存...")) saveAs();
				ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
		}
		ImGui::SetNextItemWidth(-100.0f);
		ImGui::InputText(JText(u8"パス"), &pathInput);
		ImGui::SameLine();
		if (ImGuiToolKit::Button(u8"参照...")) openFile();
		if (ImGuiToolKit::Button(ICON_FA_SAVE u8" 保存"))
		{
			if (pathInput.empty()) saveAs();
			else Save(pathInput);
		}
		ImGui::SameLine();
		if (ImGuiToolKit::Button(ICON_FA_RECYCLE u8" 読み込み"))
		{
			if (pathInput.empty()) openFile();
			else Load(pathInput);
		}
		if (!fileStatus.empty()) ImGuiToolKit::Text("%s", fileStatus.c_str());
		ImGui::Separator();

		constexpr int DisplayLayerCount = EditableLayerCount;

		auto getDisplayLayerName = [&](int index) -> std::string
		{
			const int realIndex = index;
			const std::string& name = settings.layerNames[realIndex];

			return std::to_string(index) + ": " + (name.empty() ? (JText(u8"(空)")) : name);
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
						"##LayerName", (JText(u8"(空)")), &settings.layerNames[y]))
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
	return std::to_string(id) + ": " + (name.empty() ? (JText(u8"(空)")) : name);
}
