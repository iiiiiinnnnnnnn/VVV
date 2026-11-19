#pragma once

#include "Core/Foundation/Common.h"
#include "Resource/SerialArchive.h"
#include <algorithm>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <variant>
#include <vector>

enum class VariableType : uint32_t
{
	Bool = 0,
	Int = 1,
	Float = 2,
	Vector2 = 3,
	Vector3 = 4,
	Vector4 = 5,
	Color = 6,
	String = 7
};

template <class T> struct MaterialParameter
{
	const char* shaderName;
	const char* name;
	T defaultValue;
};

class MaterialMap
{
public:
	using Value = std::variant<bool, int, float, Vector2, Vector3, Vector4, Color, std::string>;

	struct Key
	{
		VariableType type;
		std::string name;
		std::string shaderName;
		bool operator==(const Key&) const = default;
	};

	struct KeyHash
	{
		size_t operator()(const Key& key) const
		{
			size_t hash = std::hash<std::string>{}(key.shaderName);
			hash ^= std::hash<std::string>{}(key.name) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
			return hash ^ (static_cast<size_t>(key.type) + 0x9e3779b9 + (hash << 6) + (hash >> 2));
		}
	};

	template <class T> static VariableType TypeOf()
	{
		return static_cast<VariableType>(Value(T{}).index());
	}

	template <class T> const T* Find(const std::string& shaderName, const std::string& name) const
	{
		const auto found = values.find({TypeOf<T>(), name, shaderName});
		return found == values.end() ? nullptr : std::get_if<T>(&found->second);
	}

	template <class T> T& Get(const MaterialParameter<T>& parameter)
	{
		const auto found = values.try_emplace(
			Key{TypeOf<T>(), parameter.name, parameter.shaderName}, parameter.defaultValue).first;
		return std::get<T>(found->second);
	}

	template <class T> const T& Get(const MaterialParameter<T>& parameter) const
	{
		const T* value = Find<T>(parameter.shaderName, parameter.name);
		return value ? *value : parameter.defaultValue;
	}

	template <class T> void Set(const std::string& shaderName, const std::string& name, const T& value)
	{
		values.insert_or_assign(Key{TypeOf<T>(), name, shaderName}, value);
	}

	void ApplyDefaults(const MaterialMap& defaults)
	{
		for (const auto& [key, value] : defaults.values) values.try_emplace(key, value);
	}

	const auto& GetValues() const { return values; }

	template <class Archive> void serialize(Archive& archive)
	{
		std::vector<Entry> entries;
		if constexpr (!IsLoading<Archive>())
		{
			entries.reserve(values.size());
			for (const auto& [key, value] : values)
			{
				auto& entry = entries.emplace_back(Entry{key.type, key.name, key.shaderName, value});
				const auto identity = std::to_string(key.shaderName.size()) + ":" + key.shaderName +
					":" + key.name + ":" + std::to_string(static_cast<uint32_t>(key.type));
				entry.serialIdentity = SerialNode::FieldId(identity.c_str());
			}
			std::sort(entries.begin(), entries.end(), [](const Entry& left, const Entry& right)
			{
				return std::tie(left.shaderName, left.name, left.type) < std::tie(right.shaderName, right.name, right.type);
			});
		}
		archive(CEREAL_NVP(entries));
		if constexpr (IsLoading<Archive>())
		{
			decltype(values) loaded;
			for (auto& entry : entries)
				if (!loaded.emplace(Key{entry.type, entry.name, entry.shaderName}, std::move(entry.value)).second)
					throw std::runtime_error("Duplicate material parameter.");
			values = std::move(loaded);
		}
	}

private:
	template <class Archive> static constexpr bool IsLoading()
	{
		if constexpr (IsSerialArchive<Archive>) return std::is_same_v<Archive, SerialInputArchive>;
		else return Archive::is_loading::value;
	}

	struct Entry
	{
		VariableType type = VariableType::Bool;
		std::string name;
		std::string shaderName;
		Value value = false;
		uint64_t serialIdentity = 0;

		template <class Archive> void serialize(Archive& archive)
		{
			archive(CEREAL_NVP(type), CEREAL_NVP(name), CEREAL_NVP(shaderName));
			if constexpr (IsLoading<Archive>())
			{
				switch (type)
				{
				case VariableType::Bool: value = false; break;
				case VariableType::Int: value = 0; break;
				case VariableType::Float: value = 0.0f; break;
				case VariableType::Vector2: value = Vector2{}; break;
				case VariableType::Vector3: value = Vector3{}; break;
				case VariableType::Vector4: value = Vector4{}; break;
				case VariableType::Color: value = Color{}; break;
				case VariableType::String: value = std::string{}; break;
				default: throw std::runtime_error("Unsupported material parameter type.");
				}
			}
			std::visit([&](auto& stored) { archive(cereal::make_nvp("value", stored)); }, value);
		}
	};

	std::unordered_map<Key, Value, KeyHash> values;
};
