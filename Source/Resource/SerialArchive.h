#pragma once

#include "Core/Foundation/Common.h"
#include <cereal/cereal.hpp>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <filesystem>
#include <memory>
#include <limits>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <vector>

enum class SerialKind : uint32_t
{
	Object = 1, Array = 2, Bytes = 3, String = 4,
	Boolean = 5, Signed = 6, Unsigned = 7, Float = 8, Double = 9,
	Float2 = 10, Float3 = 11, Float4 = 12, Matrix = 13, UInt4 = 14,
	PackedArray = 15, Json = 16, Deleted = 0xFFFFFFFE
};

struct SerialNode
{
	uint64_t id = 0;
	std::string name;
	SerialKind kind = SerialKind::Object;
	std::vector<uint8_t> data;
	std::vector<SerialNode> children;
	bool understood = false;
	bool stableIdentity = false;
	std::string error;

	static uint64_t FieldId(const char* name);
	static uint64_t NewIdentity();
	SerialNode* Find(uint64_t field);
	SerialNode& Field(const char* name);
	uint64_t ByteSize() const;
	void Delete();
	void MarkUnderstood();
};

class SerialDocument
{
public:
	SerialNode root;
	std::string type;
	static bool IsCurrent(std::istream& stream);
	void Load(std::istream& stream, const char* expectedType);
	void Save(const std::filesystem::path& path) const;
};

class SerialOutputArchive;
class SerialInputArchive;
template <class T> void WriteSerialValue(SerialNode& node, T& value);
template <class T> void ReadSerialValue(SerialNode& node, T& value);

class SerialOutputArchive
{
public:
	explicit SerialOutputArchive(SerialNode& node) : node(node) {}
	template <class... T> void operator()(T&&... values) { (Process(std::forward<T>(values)), ...); }

private:
	template <class T> void Process(cereal::NameValuePair<T> value)
	{
		auto& field = node.Field(value.name);
		if (field.kind == SerialKind::Deleted || !field.error.empty()) return;
		auto& mutableValue = const_cast<std::remove_const_t<std::remove_reference_t<T>>&>(value.value);
		WriteSerialValue(field, mutableValue);
	}
	template <class T> void Process(T&& value)
	{
		const auto name = "value" + std::to_string(position++);
		Process(cereal::make_nvp(name.c_str(), value));
	}
	SerialNode& node;
	size_t position = 0;
};

class SerialInputArchive
{
public:
	explicit SerialInputArchive(SerialNode& node) : node(node) { node.understood = true; }
	template <class... T> void operator()(T&&... values) { (Process(std::forward<T>(values)), ...); }

private:
	template <class T> void Process(cereal::NameValuePair<T> value)
	{
		auto* field = node.Find(SerialNode::FieldId(value.name));
		if (!field) return;
		if (field->kind == SerialKind::Deleted) { field->understood = true; return; }
		try { ReadSerialValue(*field, value.value); }
		catch (const std::exception& exception)
		{
			field->understood = false;
			field->error = exception.what();
		}
	}
	template <class T> void Process(T&& value)
	{
		const auto name = "value" + std::to_string(position++);
		Process(cereal::make_nvp(name.c_str(), value));
	}
	SerialNode& node;
	size_t position = 0;
};

template <class Archive>
inline constexpr bool IsSerialArchive = std::is_same_v<Archive, SerialOutputArchive> ||
	std::is_same_v<Archive, SerialInputArchive>;

template <class T> struct SerialVector { static constexpr bool enabled = false; };
template <class T, class A> struct SerialVector<std::vector<T, A>>
{
	static constexpr bool enabled = true;
	using Element = T;
};

template <class T, bool = std::is_enum_v<T>> struct SerialNumberType { using Type = T; };
template <class T> struct SerialNumberType<T, true> { using Type = std::underlying_type_t<T>; };

template <class T> constexpr SerialKind SerialValueKind()
{
	if constexpr (std::is_same_v<T, bool>) return SerialKind::Boolean;
	else if constexpr (std::is_enum_v<T> || std::is_integral_v<T>)
		return std::is_unsigned_v<T> ? SerialKind::Unsigned : SerialKind::Signed;
	else if constexpr (std::is_same_v<T, float>) return SerialKind::Float;
	else if constexpr (std::is_floating_point_v<T>) return SerialKind::Double;
	else if constexpr (std::is_base_of_v<DirectX::XMFLOAT2, T>) return SerialKind::Float2;
	else if constexpr (std::is_base_of_v<DirectX::XMFLOAT3, T>) return SerialKind::Float3;
	else if constexpr (std::is_base_of_v<DirectX::XMFLOAT4, T>) return SerialKind::Float4;
	else if constexpr (std::is_base_of_v<DirectX::XMFLOAT4X4, T>) return SerialKind::Matrix;
	else if constexpr (std::is_same_v<T, DirectX::XMUINT4>) return SerialKind::UInt4;
	else if constexpr (std::is_same_v<T, std::string>) return SerialKind::String;
	else return SerialKind::Object;
}

template <class T> void WriteSerialValue(SerialNode& node, T& value)
{
	using U = std::remove_cv_t<T>;
	node.understood = true;
	node.error.clear();
	if constexpr (SerialVector<U>::enabled)
	{
		using E = typename SerialVector<U>::Element;
		if constexpr (std::is_same_v<E, uint8_t>)
		{
			node.kind = SerialKind::Bytes;
			node.data.assign(value.begin(), value.end());
			node.children.clear();
		}
		else if constexpr (std::is_arithmetic_v<E> && !std::is_same_v<E, bool>)
		{
			node.kind = SerialKind::PackedArray;
			const uint32_t kind = static_cast<uint32_t>(SerialValueKind<E>());
			const uint32_t stride = sizeof(E);
			const uint64_t count = value.size();
			node.data.resize(16 + count * stride);
			std::memcpy(node.data.data(), &kind, 4);
			std::memcpy(node.data.data() + 4, &stride, 4);
			std::memcpy(node.data.data() + 8, &count, 8);
			if (count) std::memcpy(node.data.data() + 16, value.data(), count * stride);
			node.children.clear();
		}
		else
		{
			node.kind = SerialKind::Array;
			node.data.clear();
			auto previous = std::move(node.children);
			std::unordered_set<uint64_t> used;
			for (size_t i = 0; i < value.size(); ++i)
			{
				auto& element = value[i];
				uint64_t id = i + 1;
				if constexpr (requires { element.serialIdentity; })
				{
					if (!element.serialIdentity)
					{
						do { element.serialIdentity = SerialNode::NewIdentity(); }
						while (std::any_of(previous.begin(), previous.end(), [&](const auto& item) { return item.id == element.serialIdentity; }));
					}
					id = element.serialIdentity;
				}
				auto old = std::find_if(previous.begin(), previous.end(), [&](const auto& child) { return child.id == id; });
				SerialNode child;
				if (old != previous.end()) child = *old;
				if (!used.insert(id).second)
				{
					do { id = SerialNode::NewIdentity(); }
					while (used.contains(id) || std::any_of(previous.begin(), previous.end(), [&](const auto& item) { return item.id == id; }));
					used.insert(id);
					if constexpr (requires { element.serialIdentity; }) element.serialIdentity = id;
				}
				child.id = id;
				if constexpr (requires { element.serialIdentity; }) child.stableIdentity = true;
				child.name = "[" + std::to_string(i) + "]";
				if constexpr (requires { element.name; }) child.name = element.name;
				if (child.kind != SerialKind::Deleted && child.error.empty()) WriteSerialValue(child, element);
				node.children.push_back(std::move(child));
			}
			for (auto& old : previous)
				if (!used.contains(old.id) && (!old.understood || old.kind == SerialKind::Deleted))
					node.children.push_back(std::move(old));
		}
	}
	else if constexpr (std::is_same_v<U, std::string>)
	{
		node.kind = SerialKind::String;
		node.data.assign(value.begin(), value.end());
		node.children.clear();
	}
	else if constexpr (SerialValueKind<U>() != SerialKind::Object)
	{
		node.kind = SerialValueKind<U>();
		node.children.clear();
		if constexpr (std::is_integral_v<U> || std::is_enum_v<U>)
		{
			const uint64_t bits = static_cast<uint64_t>(value);
			node.data.resize(sizeof(bits));
			std::memcpy(node.data.data(), &bits, sizeof(bits));
		}
		else
		{
			node.data.resize(sizeof(U));
			std::memcpy(node.data.data(), &value, sizeof(U));
		}
	}
	else
	{
		node.kind = SerialKind::Object;
		node.data.clear();
		SerialOutputArchive archive(node);
		cereal::access::member_serialize(archive, value);
	}
}

template <class T> void ReadSerialValue(SerialNode& node, T& value)
{
	using U = std::remove_cv_t<T>;
	if constexpr (SerialVector<U>::enabled)
	{
		using E = typename SerialVector<U>::Element;
		if constexpr (std::is_same_v<E, uint8_t>)
		{
			if (node.kind != SerialKind::Bytes) throw std::runtime_error("Byte array type mismatch.");
			value.assign(node.data.begin(), node.data.end());
		}
		else if constexpr (std::is_arithmetic_v<E> && !std::is_same_v<E, bool>)
		{
			if (node.kind != SerialKind::PackedArray || node.data.size() < 16) throw std::runtime_error("Array type mismatch.");
			uint32_t kind, stride;
			uint64_t count;
			std::memcpy(&kind, node.data.data(), 4);
			std::memcpy(&stride, node.data.data() + 4, 4);
			std::memcpy(&count, node.data.data() + 8, 8);
			if (kind != static_cast<uint32_t>(SerialValueKind<E>()) || stride != sizeof(E) || count != (node.data.size() - 16) / sizeof(E) || (node.data.size() - 16) % sizeof(E))
				throw std::runtime_error("Invalid packed array.");
			value.resize(static_cast<size_t>(count));
			if (count) std::memcpy(value.data(), node.data.data() + 16, count * sizeof(E));
		}
		else
		{
			if (node.kind != SerialKind::Array) throw std::runtime_error("Array type mismatch.");
			value.clear();
			for (auto& child : node.children)
			{
				if (child.kind == SerialKind::Deleted) { child.understood = true; continue; }
				E element{};
				try
				{
					ReadSerialValue(child, element);
					if constexpr (requires { element.serialIdentity; }) { element.serialIdentity = child.id; child.stableIdentity = true; }
					value.push_back(std::move(element));
				}
				catch (const std::exception& exception) { child.understood = false; child.error = exception.what(); }
			}
		}
	}
	else if constexpr (std::is_same_v<U, std::string>)
	{
		if (node.kind != SerialKind::String) throw std::runtime_error("String type mismatch.");
		value.assign(node.data.begin(), node.data.end());
	}
	else if constexpr (SerialValueKind<U>() != SerialKind::Object)
	{
		if (node.kind != SerialValueKind<U>()) throw std::runtime_error("Value type mismatch.");
		if constexpr (std::is_integral_v<U> || std::is_enum_v<U>)
		{
			if (node.data.size() != 8) throw std::runtime_error("Invalid integer size.");
			uint64_t bits;
			std::memcpy(&bits, node.data.data(), 8);
			using Number = typename SerialNumberType<U>::Type;
			if constexpr (std::is_unsigned_v<Number> || std::is_same_v<Number, bool>)
			{
				if (bits > static_cast<uint64_t>(std::numeric_limits<Number>::max())) throw std::runtime_error("Integer is outside the supported range.");
			}
			else
			{
				const auto number = static_cast<int64_t>(bits);
				if (number < std::numeric_limits<Number>::min() || number > std::numeric_limits<Number>::max())
					throw std::runtime_error("Integer is outside the supported range.");
			}
			value = static_cast<U>(bits);
		}
		else
		{
			if (node.data.size() != sizeof(U)) throw std::runtime_error("Invalid value size.");
			std::memcpy(&value, node.data.data(), sizeof(U));
		}
	}
	else
	{
		if (node.kind != SerialKind::Object) throw std::runtime_error("Object type mismatch.");
		SerialInputArchive archive(node);
		cereal::access::member_serialize(archive, value);
	}
	node.understood = true;
	node.error.clear();
}
