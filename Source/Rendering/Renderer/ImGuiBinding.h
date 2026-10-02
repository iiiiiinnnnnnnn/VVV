#pragma once

#include <SimpleMath.h>

#include <string>
#include <type_traits>
#include <utility>
#include <variant>

#ifndef _RELEASE
#include <imgui.h>

#include <cstring>
#include <map>
#include <source_location>
#include <string_view>
#endif

class ImGuiBinding
{
#ifdef _RELEASE
  public:
	template <typename T>
		requires std::is_arithmetic_v<std::remove_cv_t<T>>
	ImGuiBinding(T defaultValue)
	{
		using ValueType = std::remove_cv_t<T>;

		if constexpr (std::is_same_v<ValueType, bool>) value_ = static_cast<bool>(defaultValue);
		else if constexpr (std::is_floating_point_v<ValueType>)
		{
			if constexpr (sizeof(ValueType) <= sizeof(float))
				value_ = static_cast<float>(defaultValue);
			else value_ = static_cast<double>(defaultValue);
		}
		else value_ = static_cast<int>(defaultValue);
	}

	ImGuiBinding(const std::string& defaultValue) : value_(defaultValue) {}

	ImGuiBinding(std::string&& defaultValue) : value_(std::move(defaultValue)) {}

	ImGuiBinding(const char* defaultValue) : value_(std::string(defaultValue ? defaultValue : ""))
	{}

	ImGuiBinding(const DirectX::SimpleMath::Vector2& defaultValue) : value_(defaultValue) {}

	ImGuiBinding(const DirectX::SimpleMath::Vector3& defaultValue) : value_(defaultValue) {}

	ImGuiBinding(const DirectX::SimpleMath::Quaternion& defaultValue) : value_(defaultValue) {}

	operator int&() { return GetValue<int>(); }

	operator float&() { return GetValue<float>(); }

	operator double&() { return GetValue<double>(); }

	operator bool&() { return GetValue<bool>(); }

	operator std::string&() { return std::get<std::string>(value_); }

	operator DirectX::SimpleMath::Vector2&()
	{
		return std::get<DirectX::SimpleMath::Vector2>(value_);
	}

	operator DirectX::SimpleMath::Vector3&()
	{
		return std::get<DirectX::SimpleMath::Vector3>(value_);
	}

	operator DirectX::SimpleMath::Quaternion&()
	{
		return std::get<DirectX::SimpleMath::Quaternion>(value_);
	}

	static void Draw() {}

  private:
	using Value = std::variant<int, float, double, bool, std::string, DirectX::SimpleMath::Vector2,
		DirectX::SimpleMath::Vector3, DirectX::SimpleMath::Quaternion>;

	template <typename T> T& GetValue()
	{
		if (auto* value = std::get_if<T>(&value_)) return *value;

		T convertedValue{};
		std::visit(
			[&convertedValue](const auto& value) {
				using SourceType = std::decay_t<decltype(value)>;
				if constexpr (std::is_arithmetic_v<SourceType>)
					convertedValue = static_cast<T>(value);
			},
			value_);

		value_ = convertedValue;
		return std::get<T>(value_);
	}

	Value value_ = 0;
#else
  public:
	template <typename T>
		requires std::is_arithmetic_v<std::remove_cv_t<T>>
	ImGuiBinding(T defaultValue, std::source_location location = std::source_location::current())
		: defaultValue_(static_cast<long double>(defaultValue)), key_(MakeKey(location)),
		  label_(MakeLabel(location))
	{}

	ImGuiBinding(const std::string& defaultValue,
		std::source_location location = std::source_location::current())
		: defaultValue_(defaultValue), key_(MakeKey(location)), label_(MakeLabel(location))
	{}

	ImGuiBinding(
		std::string&& defaultValue, std::source_location location = std::source_location::current())
		: defaultValue_(std::move(defaultValue)), key_(MakeKey(location)),
		  label_(MakeLabel(location))
	{}

	ImGuiBinding(
		const char* defaultValue, std::source_location location = std::source_location::current())
		: defaultValue_(std::string(defaultValue ? defaultValue : "")), key_(MakeKey(location)),
		  label_(MakeLabel(location))
	{}

	ImGuiBinding(const DirectX::SimpleMath::Vector2& defaultValue,
		std::source_location location = std::source_location::current())
		: defaultValue_(defaultValue), key_(MakeKey(location)), label_(MakeLabel(location))
	{}

	ImGuiBinding(const DirectX::SimpleMath::Vector3& defaultValue,
		std::source_location location = std::source_location::current())
		: defaultValue_(defaultValue), key_(MakeKey(location)), label_(MakeLabel(location))
	{}

	ImGuiBinding(const DirectX::SimpleMath::Quaternion& defaultValue,
		std::source_location location = std::source_location::current())
		: defaultValue_(defaultValue), key_(MakeKey(location)), label_(MakeLabel(location))
	{}

	operator int&() { return GetValue<int>(); }

	operator float&() { return GetValue<float>(); }

	operator double&() { return GetValue<double>(); }

	operator bool&() { return GetValue<bool>(); }

	operator std::string&() { return GetValue<std::string>(); }

	operator DirectX::SimpleMath::Vector2&() { return GetValue<DirectX::SimpleMath::Vector2>(); }

	operator DirectX::SimpleMath::Vector3&() { return GetValue<DirectX::SimpleMath::Vector3>(); }

	operator DirectX::SimpleMath::Quaternion&()
	{
		return GetValue<DirectX::SimpleMath::Quaternion>();
	}

	static void Draw()
	{
		if (!ImGui::Begin("ImGui Bindings"))
		{
			ImGui::End();
			return;
		}

		for (auto& [key, entry] : Registry())
		{
			ImGui::PushID(key.c_str());
			ImGui::TextUnformatted(entry.label.c_str());
			ImGui::SameLine();
			ImGui::SetNextItemWidth(280.0f);

			switch (entry.type)
			{
			case ValueType::Int:
				ImGui::DragInt("##Value", &std::get<int>(entry.value), 1.0f);
				break;

			case ValueType::Float:
				ImGui::DragFloat("##Value", &std::get<float>(entry.value), 0.01f);
				break;

			case ValueType::Double:
				ImGui::DragScalar(
					"##Value", ImGuiDataType_Double, &std::get<double>(entry.value), 0.01f);
				break;

			case ValueType::Bool:
				ImGui::Checkbox("##Value", &std::get<bool>(entry.value));
				break;

			case ValueType::String:
				InputTextString("##Value", std::get<std::string>(entry.value));
				break;

			case ValueType::Vector2:
			{
				auto& value = std::get<DirectX::SimpleMath::Vector2>(entry.value);
				ImGui::DragFloat2("##Value", &value.x, 0.01f);
				break;
			}

			case ValueType::Vector3:
			{
				auto& value = std::get<DirectX::SimpleMath::Vector3>(entry.value);
				ImGui::DragFloat3("##Value", &value.x, 0.01f);
				break;
			}

			case ValueType::Quaternion:
			{
				auto& value = std::get<DirectX::SimpleMath::Quaternion>(entry.value);
				DirectX::SimpleMath::Vector3 euler = value.ToEuler();

				float degrees[3]{
					DirectX::XMConvertToDegrees(euler.x),
					DirectX::XMConvertToDegrees(euler.y),
					DirectX::XMConvertToDegrees(euler.z),
				};

				if (ImGui::DragFloat3("##Value", degrees, 0.1f))
				{
					euler.x = DirectX::XMConvertToRadians(degrees[0]);
					euler.y = DirectX::XMConvertToRadians(degrees[1]);
					euler.z = DirectX::XMConvertToRadians(degrees[2]);
					value = DirectX::SimpleMath::Quaternion::CreateFromYawPitchRoll(euler);
				}
				break;
			}
			}

			ImGui::PopID();
		}

		ImGui::End();
	}

  private:
	enum class ValueType
	{
		Int,
		Float,
		Double,
		Bool,
		String,
		Vector2,
		Vector3,
		Quaternion,
	};

	using Value = std::variant<int, float, double, bool, std::string, DirectX::SimpleMath::Vector2,
		DirectX::SimpleMath::Vector3, DirectX::SimpleMath::Quaternion>;

	using DefaultValue = std::variant<long double, std::string, DirectX::SimpleMath::Vector2,
		DirectX::SimpleMath::Vector3, DirectX::SimpleMath::Quaternion>;

	struct Entry
	{
		ValueType type = ValueType::Int;
		Value value = 0;
		std::string label;
	};

	template <typename T> static constexpr ValueType GetValueType()
	{
		if constexpr (std::is_same_v<T, int>) return ValueType::Int;
		else if constexpr (std::is_same_v<T, float>) return ValueType::Float;
		else if constexpr (std::is_same_v<T, double>) return ValueType::Double;
		else if constexpr (std::is_same_v<T, bool>) return ValueType::Bool;
		else if constexpr (std::is_same_v<T, std::string>) return ValueType::String;
		else if constexpr (std::is_same_v<T, DirectX::SimpleMath::Vector2>)
			return ValueType::Vector2;
		else if constexpr (std::is_same_v<T, DirectX::SimpleMath::Vector3>)
			return ValueType::Vector3;
		else return ValueType::Quaternion;
	}

	template <typename T> T GetDefaultValue() const
	{
		if constexpr (std::is_arithmetic_v<T>)
		{
			if (const auto* value = std::get_if<long double>(&defaultValue_))
				return static_cast<T>(*value);

			return T{};
		}
		else
		{
			if (const auto* value = std::get_if<T>(&defaultValue_)) return *value;

			return T{};
		}
	}

	template <typename T> T& GetValue()
	{
		auto& registry = Registry();
		auto it = registry.find(key_);

		if (it == registry.end())
		{
			Entry entry;
			entry.type = GetValueType<T>();
			entry.value = GetDefaultValue<T>();
			entry.label = label_;
			it = registry.emplace(key_, std::move(entry)).first;
		}
		else if (it->second.type != GetValueType<T>())
		{
			it->second.type = GetValueType<T>();
			it->second.value = GetDefaultValue<T>();
		}

		return std::get<T>(it->second.value);
	}

	static int InputTextResizeCallback(ImGuiInputTextCallbackData* data)
	{
		if (data->EventFlag != ImGuiInputTextFlags_CallbackResize) return 0;

		auto* value = static_cast<std::string*>(data->UserData);
		value->resize(static_cast<std::size_t>(data->BufTextLen));
		data->Buf = value->data();
		return 0;
	}

	static bool InputTextString(const char* label, std::string& value)
	{
		if (value.capacity() < 32) value.reserve(32);

		const bool changed = ImGui::InputText(label, value.data(), value.capacity() + 1,
			ImGuiInputTextFlags_CallbackResize, InputTextResizeCallback, &value);

		if (changed) value.resize(std::strlen(value.c_str()));

		return changed;
	}

	static std::map<std::string, Entry>& Registry()
	{
		static std::map<std::string, Entry> registry;
		return registry;
	}

	static std::string MakeKey(const std::source_location& location)
	{
		std::string key;
		key.reserve(128);
		key += location.file_name();
		key += ':';
		key += std::to_string(location.line());
		key += ':';
		key += std::to_string(location.column());
		return key;
	}

	static std::string MakeLabel(const std::source_location& location)
	{
		std::string_view fileName = location.file_name();
		const std::size_t slash = fileName.find_last_of("/\\");
		if (slash != std::string_view::npos) fileName.remove_prefix(slash + 1);

		std::string label(fileName);
		label += ':';
		label += std::to_string(location.line());
		return label;
	}

	DefaultValue defaultValue_ = static_cast<long double>(0.0L);
	std::string key_;
	std::string label_;
#endif
};

using ImGuiBind = ImGuiBinding;
