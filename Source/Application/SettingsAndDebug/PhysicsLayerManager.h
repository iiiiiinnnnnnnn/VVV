// PhysicsLayerManager.h

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

class PhysicsLayerManager
{
public:
	static PhysicsLayerManager& Instance()
	{
		static PhysicsLayerManager instance;
		return instance;
	}

	struct PhysicsLayers
	{
		std::array<std::string, EditableLayerCount> layerNames;
		std::array<std::array<uint8_t, EditableLayerCount>, EditableLayerCount> collisionMatrix;

		template<class Archive>
		void serialize(Archive& archive);
	};

	void Initialize();
	void DrawGUI(bool* open = nullptr);

	// 受け取り

	bool Collides(LayerId a, LayerId b) const;

	void SetCollides(LayerId a, LayerId b, bool enabled);

	LayerId GetLayerId(const char* name) const;
	const std::string& GetLayerName(LayerId id) const;
	std::string GetLayerDisplayName(LayerId id) const;

private:
	PhysicsLayerManager() = default;
	~PhysicsLayerManager() = default;

	// 読み書き

	bool Load();
	bool Save() const;
	bool Validate();
	void CreateDefault();

	PhysicsLayers settings;
};

// ヘルパー

namespace Layers
{
	inline LayerId Get(const char* name)
	{
		return PhysicsLayerManager::Instance().GetLayerId(name);
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
