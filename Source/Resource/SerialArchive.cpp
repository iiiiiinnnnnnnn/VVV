#include "Resource/SerialArchive.h"
#include "Resource/SerialJson.h"
#include <compressapi.h>
#include <array>
#include <fstream>
#include <sstream>

uint64_t SerialNode::FieldId(const char* name)
{
	uint64_t hash = 14695981039346656037ull;
	for (; *name; ++name) { hash ^= static_cast<uint8_t>(*name); hash *= 1099511628211ull; }
	return hash & 0x7FFFFFFFFFFFFFFFull;
}

uint64_t SerialNode::NewIdentity()
{
	static std::atomic<uint64_t> next = 1;
	return 0x8000000000000000ull | next.fetch_add(1, std::memory_order_relaxed);
}

SerialNode* SerialNode::Find(uint64_t field)
{
	for (auto& child : children) if (child.id == field) return &child;
	return nullptr;
}

SerialNode& SerialNode::Field(const char* fieldName)
{
	const auto field = FieldId(fieldName);
	if (auto* existing = Find(field))
	{
		if (existing->name != fieldName) throw std::runtime_error("Serial field ID collision.");
		return *existing;
	}
	auto& result = children.emplace_back();
	result.id = field;
	result.name = fieldName;
	return result;
}

uint64_t SerialNode::ByteSize() const
{
	uint64_t size = data.size();
	if (kind == SerialKind::Object || kind == SerialKind::Array)
	{
		size = 8;
		for (const auto& child : children) size += 24 + child.name.size() + child.ByteSize();
	}
	return size;
}

void SerialNode::Delete()
{
	kind = SerialKind::Deleted;
	data.clear();
	children.clear();
	understood = true;
	error.clear();
}

void SerialNode::MarkUnderstood()
{
	understood = true;
	error.clear();
	for (auto& child : children) child.MarkUnderstood();
}

class SerialWire
{
public:
	inline static constexpr std::array<char, 8> magic = {'V','S','E','R','I','A','L','\0'};
	static constexpr uint64_t maximumBytes = 2ull * 1024 * 1024 * 1024;
	template <class T> static void Write(std::ostream& stream, const T& value)
	{
		stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
		if (!stream) throw std::runtime_error("Serial write failed.");
	}
	template <class T> static T Read(std::istream& stream)
	{
		T value{};
		stream.read(reinterpret_cast<char*>(&value), sizeof(value));
		if (!stream) throw std::runtime_error("Serial data is truncated.");
		return value;
	}
	static void WriteNode(std::ostream& stream, const SerialNode& node)
	{
		Write(stream, node.id);
		Write(stream, static_cast<uint32_t>(node.kind));
		Write(stream, static_cast<uint32_t>(node.name.size()));
		stream.write(node.name.data(), node.name.size());
		Write(stream, node.ByteSize());
		if (node.kind == SerialKind::Object || node.kind == SerialKind::Array)
		{
			Write(stream, static_cast<uint64_t>(node.children.size()));
			for (const auto& child : node.children) WriteNode(stream, child);
		}
		else if (!node.data.empty()) stream.write(reinterpret_cast<const char*>(node.data.data()), node.data.size());
		if (!stream) throw std::runtime_error("Serial payload write failed.");
	}
	static SerialNode ReadNode(std::istream& stream, uint64_t end, size_t depth, size_t& count)
	{
		const auto position = static_cast<uint64_t>(stream.tellg());
		if (depth > 64 || ++count > 1000000 || position > end || end - position < 24)
			throw std::runtime_error("Invalid serial node bounds.");
		SerialNode node;
		node.id = Read<uint64_t>(stream);
		node.kind = static_cast<SerialKind>(Read<uint32_t>(stream));
		const auto nameSize = Read<uint32_t>(stream);
		if (nameSize > 4096 || nameSize > end - position - 24) throw std::runtime_error("Invalid serial name length.");
		node.name.resize(nameSize);
		stream.read(node.name.data(), nameSize);
		const auto size = Read<uint64_t>(stream);
		const auto start = static_cast<uint64_t>(stream.tellg());
		if (start > end || size > end - start) throw std::runtime_error("Invalid serial payload length.");
		const auto payloadEnd = start + size;
		if (node.kind == SerialKind::Object || node.kind == SerialKind::Array)
		{
			if (size < 8) throw std::runtime_error("Invalid serial object.");
			const auto children = Read<uint64_t>(stream);
			if (children > (size - 8) / 24 || children > 1000000 - count) throw std::runtime_error("Invalid serial item count.");
			std::unordered_set<uint64_t> ids;
			for (uint64_t i = 0; i < children; ++i)
			{
				auto child = ReadNode(stream, payloadEnd, depth + 1, count);
				if (!ids.insert(child.id).second) throw std::runtime_error("Duplicate serial ID.");
				node.children.push_back(std::move(child));
			}
		}
		else
		{
			node.data.resize(static_cast<size_t>(size));
			if (size) stream.read(reinterpret_cast<char*>(node.data.data()), static_cast<std::streamsize>(size));
		}
		if (!stream || static_cast<uint64_t>(stream.tellg()) != payloadEnd) throw std::runtime_error("Serial payload size mismatch.");
		return node;
	}
};

bool SerialDocument::IsCurrent(std::istream& stream)
{
	const auto position = stream.tellg();
	std::array<char, 8> magic{};
	stream.read(magic.data(), magic.size());
	const bool result = stream.gcount() == magic.size() && magic == SerialWire::magic;
	stream.clear();
	stream.seekg(position);
	return result;
}

void SerialDocument::Load(std::istream& stream, const char* expectedType)
{
	std::array<char, 8> magic{};
	stream.read(magic.data(), magic.size());
	char fileType[4]{};
	stream.read(fileType, sizeof(fileType));
	const auto codec = SerialWire::Read<uint32_t>(stream);
	const auto plainSize = SerialWire::Read<uint64_t>(stream);
	const auto storedSize = SerialWire::Read<uint64_t>(stream);
	if (magic != SerialWire::magic || std::string(fileType, 4) != expectedType || codec != 1 ||
		!plainSize || plainSize > SerialWire::maximumBytes || !storedSize || storedSize > SerialWire::maximumBytes)
		throw std::runtime_error("Invalid serial container header.");
	const auto start = stream.tellg();
	stream.seekg(0, std::ios::end);
	if (stream.tellg() - start != static_cast<std::streamoff>(storedSize)) throw std::runtime_error("Invalid serial container size.");
	stream.seekg(start);
	std::vector<uint8_t> stored(static_cast<size_t>(storedSize));
	stream.read(reinterpret_cast<char*>(stored.data()), stored.size());
	std::string plain(static_cast<size_t>(plainSize), '\0');
	DECOMPRESSOR_HANDLE handle = nullptr;
	if (!CreateDecompressor(COMPRESS_ALGORITHM_XPRESS_HUFF, nullptr, &handle)) throw std::runtime_error("Serial decompressor creation failed.");
	SIZE_T actual = 0;
	const BOOL result = Decompress(handle, stored.data(), stored.size(), plain.data(), plain.size(), &actual);
	CloseDecompressor(handle);
	if (!result || actual != plain.size()) throw std::runtime_error("Serial decompression failed.");
	std::istringstream payload(plain, std::ios::binary);
	size_t count = 0;
	auto loaded = SerialWire::ReadNode(payload, plain.size(), 0, count);
	if (static_cast<uint64_t>(payload.tellg()) != plain.size()) throw std::runtime_error("Unexpected serial trailing bytes.");
	root = std::move(loaded);
	type = expectedType;
}

void SerialDocument::Save(const std::filesystem::path& path) const
{
	if (type != "VMDL" && type != "VSTG") throw std::runtime_error("Invalid serial document type.");
	std::ostringstream payload(std::ios::binary);
	SerialWire::WriteNode(payload, root);
	const auto plain = payload.str();
	COMPRESSOR_HANDLE handle = nullptr;
	if (!CreateCompressor(COMPRESS_ALGORITHM_XPRESS_HUFF, nullptr, &handle)) throw std::runtime_error("Serial compressor creation failed.");
	SIZE_T size = 0;
	Compress(handle, plain.data(), plain.size(), nullptr, 0, &size);
	if (!size) { CloseCompressor(handle); throw std::runtime_error("Serial compression size failed."); }
	std::vector<uint8_t> stored(size);
	const BOOL result = Compress(handle, plain.data(), plain.size(), stored.data(), stored.size(), &size);
	CloseCompressor(handle);
	if (!result) throw std::runtime_error("Serial compression failed.");
	if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
	const auto temporary = std::filesystem::path(path.wstring() + L".saving.tmp");
	try
	{
		std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
		stream.write(SerialWire::magic.data(), SerialWire::magic.size());
		stream.write(type.data(), 4);
		SerialWire::Write(stream, uint32_t(1)); // 圧縮方式ID。ファイルの世代番号ではない。
		SerialWire::Write(stream, static_cast<uint64_t>(plain.size()));
		SerialWire::Write(stream, static_cast<uint64_t>(size));
		stream.write(reinterpret_cast<const char*>(stored.data()), size);
		stream.close();
		if (!stream || !MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
			throw std::runtime_error("Serial file replacement failed: " + std::to_string(GetLastError()));
	}
	catch (...)
	{
		std::error_code ignored;
		std::filesystem::remove(temporary, ignored);
		throw;
	}
}

void WriteSerialJson(SerialNode& node, const json& value)
{
	if (node.kind == SerialKind::Deleted || !node.error.empty()) return;
	node.understood = true;
	if (value.is_object())
	{
		node.kind = SerialKind::Object;
		node.data.clear();
		for (auto it = value.begin(); it != value.end(); ++it) WriteSerialJson(node.Field(it.key().c_str()), it.value());
	}
	else if (value.is_array())
	{
		node.kind = SerialKind::Array;
		node.data.clear();
		auto old = std::move(node.children);
		std::vector<size_t> usable;
		for (size_t i = 0; i < old.size(); ++i)
			if (old[i].kind != SerialKind::Deleted && old[i].error.empty()) usable.push_back(i);
		std::unordered_set<uint64_t> used;
		for (size_t i = 0; i < value.size(); ++i)
		{
			SerialNode child;
			if (i < usable.size()) child = std::move(old[usable[i]]);
			else
			{
				do { child.id = SerialNode::NewIdentity(); }
				while (std::any_of(old.begin(), old.end(), [&](const auto& previous) { return previous.id == child.id; }));
			}
			used.insert(child.id);
			child.name = "[" + std::to_string(i) + "]";
			WriteSerialJson(child, value[i]);
			node.children.push_back(std::move(child));
		}
		for (auto& previous : old)
			if (!used.contains(previous.id) && (!previous.understood || previous.kind == SerialKind::Deleted || !previous.error.empty()))
				node.children.push_back(std::move(previous));
	}
	else
	{
		node.kind = SerialKind::Json;
		node.children.clear();
		const auto text = value.dump();
		node.data.assign(text.begin(), text.end());
	}
}

json ReadSerialJson(SerialNode& node)
{
	if (node.kind == SerialKind::Object)
	{
		json result = json::object();
		for (auto& child : node.children)
		{
			if (child.kind == SerialKind::Deleted) continue;
			try { result[child.name] = ReadSerialJson(child); }
			catch (const std::exception& exception) { child.error = exception.what(); }
		}
		return result;
	}
	if (node.kind == SerialKind::Array)
	{
		json result = json::array();
		for (auto& child : node.children)
		{
			if (child.kind == SerialKind::Deleted) continue;
			try { result.push_back(ReadSerialJson(child)); }
			catch (const std::exception& exception) { child.error = exception.what(); }
		}
		return result;
	}
	if (node.kind != SerialKind::Json) throw std::runtime_error("Unknown JSON serial value type.");
	return json::parse(node.data.begin(), node.data.end());
}
