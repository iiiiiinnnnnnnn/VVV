// UserSettingsManager.h

// エンジン内部の設定

#pragma once

#include <array>
#include <cstdint>
#include <string>

using LayerId = uint8_t;

// レイヤーは20個まで
constexpr int EditableLayerCount = 20;

constexpr LayerId InvalidLayerId = 255;
constexpr LayerId EverythingLayerId = 254;
constexpr LayerId NothingLayerId = 253;

class UserSettingsManager
{
public:
	static UserSettingsManager& Instance()
	{
		static UserSettingsManager instance;
		return instance;
	}

	struct UserSettings
	{
		std::array<std::string, EditableLayerCount> layerNames;
		std::array<std::array<uint8_t, EditableLayerCount>, EditableLayerCount> collisionMatrix;

		template<class Archive>
		void serialize(Archive& archive);
	};

	void Initialize();
	void DrawGUI();

	// 受け取り

	bool Collides(LayerId a, LayerId b) const;

	void SetCollides(LayerId a, LayerId b, bool enabled);

	LayerId GetLayerId(const char* name) const;

private:
	UserSettingsManager() = default;
	~UserSettingsManager() = default;

	// 読み書き

	bool Load();
	bool Save() const;
	bool Validate();
	void CreateDefault();

	UserSettings settings;
};

// ヘルパー

namespace Layers
{
	inline LayerId Get(const char* name)
	{
		return UserSettingsManager::Instance().GetLayerId(name);
	}
	inline LayerId Everything()
	{
		return EverythingLayerId;
	}

	inline LayerId Nothing()
	{
		return NothingLayerId;
	}
}