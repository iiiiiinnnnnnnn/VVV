// UserSettingsManager.cpp

// UseSettingsManager.cpp

#include "Application/SettingsAndDebug/UserSettingsManager.h"
#include <fstream>

// Cereal
#include <cereal/cereal.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/utility.hpp>
#include <cereal/types/array.hpp>

#include "imgui.h"

constexpr const char* filepath = "Data/UserSettings.vvv";

template<class Archive>
void UserSettingsManager::UserSettings::serialize(Archive& archive)
{
    archive(
        CEREAL_NVP(layerNames),
        CEREAL_NVP(collisionMatrix)
    );
}

bool UserSettingsManager::Load()
{
    std::ifstream ifs(filepath, std::ios::binary);
    if (!ifs)
    {
		CreateDefault();
        return false;
    }

    cereal::BinaryInputArchive archive(ifs);
    archive(CEREAL_NVP(settings));

    return true;
}

bool UserSettingsManager::Save() const
{
    std::ofstream ofs(filepath, std::ios::binary);
    if (!ofs) return false;

    cereal::BinaryOutputArchive archive(ofs);
    archive(CEREAL_NVP(settings));

    return true;
}

void UserSettingsManager::CreateDefault()
{
    settings.layerNames =
    {
        "Default",
        "Player",
        "Enemy",
        "Stage",
        "PlayerAttack",
        "EnemyAttack",
        "OnStage",
        "Prop",
        "Item",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        ""
    };

    for (int y = 0; y < EditableLayerCount; ++y)
    {
        for (int x = 0; x < EditableLayerCount; ++x)
        {
            settings.collisionMatrix[y][x] = 0;
        }
    }

    auto set = [&](int a, int b)
    {
        settings.collisionMatrix[a][b] = 1;
        settings.collisionMatrix[b][a] = 1;
    };

    const int Default      = 0;
    const int Player       = 1;
    const int Enemy        = 2;
    const int Stage        = 3;
    const int PlayerAttack = 4;
    const int EnemyAttack  = 5;
    const int OnStage      = 6;
    const int Prop         = 7;
    const int Item         = 8;

    set(Default, Default);
    set(Default, Player);
    set(Default, Enemy);
    set(Default, Stage);
    set(Default, Prop);
    set(Default, Item);

    set(Player, Enemy);
    set(Player, Stage);
    set(Player, EnemyAttack);
    set(Player, OnStage);
    set(Player, Prop);
    set(Player, Item);

    set(Enemy, Stage);
    set(Enemy, PlayerAttack);
    set(Enemy, OnStage);
    set(Enemy, Prop);

    set(Stage, Player);
    set(Stage, Enemy);
    set(Stage, PlayerAttack);
    set(Stage, EnemyAttack);
    set(Stage, OnStage);
    set(Stage, Prop);
    set(Stage, Item);

    set(PlayerAttack, Enemy);
    set(PlayerAttack, Stage);
    set(PlayerAttack, Prop);

    set(EnemyAttack, Player);
    set(EnemyAttack, Stage);
    set(EnemyAttack, Prop);

    set(OnStage, Player);
    set(OnStage, Enemy);
    set(OnStage, Stage);

    set(Prop, Default);
    set(Prop, Player);
    set(Prop, Enemy);
    set(Prop, Stage);
    set(Prop, PlayerAttack);
    set(Prop, EnemyAttack);
    set(Prop, Item);

    set(Item, Player);
    set(Item, Stage);
    set(Item, Prop);
}

bool UserSettingsManager::Collides(LayerId a, LayerId b) const
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

void UserSettingsManager::SetCollides(LayerId a, LayerId b, bool enabled)
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
}

void UserSettingsManager::Initialize()
{
    Load();
}

void UserSettingsManager::DrawGUI()
{
    if (ImGui::Begin("UserSettingsManager"))
    {
        if (ImGui::Button("Save"))
        {
            Save();
        }

        ImGui::SameLine();

        if (ImGui::Button("Reload"))
        {
            Load();
        }

        ImGui::SameLine();

        if (ImGui::Button("Create Default"))
        {
            CreateDefault();
            Save();
        }

        ImGui::Separator();

        constexpr int DisplayLayerCount = EditableLayerCount;

        auto getDisplayLayerName = [&](int index) -> const char*
        {
            const int realIndex = index;
            const std::string& name = settings.layerNames[realIndex];

            return name.empty() ? "(Empty)" : name.c_str();
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
            constexpr float LayerColumnWidth = 80.0f;
            constexpr float CollisionColumnWidth = 80.0f;

            ImGui::TableSetupColumn(
                "Layer",
                ImGuiTableColumnFlags_WidthFixed,
                LayerColumnWidth);

            for (int x = 0; x < DisplayLayerCount; ++x)
            {
                ImGui::TableSetupColumn(
                    getDisplayLayerName(x),
                    ImGuiTableColumnFlags_WidthFixed,
                    CollisionColumnWidth);
            }

            ImGui::TableHeadersRow();

            for (int y = 0; y < DisplayLayerCount; ++y)
            {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);

                char name[64];
                strcpy_s(name, settings.layerNames[y].c_str());

                char nameId[64];
                sprintf_s(nameId, "##LayerName_%d", y);

                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

                if (ImGui::InputText(nameId, name, sizeof(name)))
                {
                    settings.layerNames[y] = name;
                }

                for (int x = 0; x < DisplayLayerCount; ++x)
                {
                    ImGui::TableSetColumnIndex(x + 1);

                    const LayerId layerA = getDisplayLayerId(y);
                    const LayerId layerB = getDisplayLayerId(x);

                    bool enabled = Collides(layerA, layerB);

                    char id[64];
                    sprintf_s(id, "##Collision_%d_%d", y, x);

                    if (ImGui::Checkbox(id, &enabled))
                    {
                        SetCollides(layerA, layerB, enabled);
                    }
                }
            }

            ImGui::EndTable();
        }
    }

    ImGui::End();
}

LayerId UserSettingsManager::GetLayerId(const char* name) const
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