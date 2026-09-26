// Effect.cpp
#include "Rendering/Core/Graphics.h"
#include "Rendering/Effect/Effect.h"
#include "Rendering/Effect/EffectManager.h"
#include "Core/Foundation/Json.h"
#include <stb_image.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
constexpr uint32_t ZipLocalHeader = 0x04034B50;
constexpr uint32_t ZipCentralHeader = 0x02014B50;
constexpr uint32_t ZipEndHeader = 0x06054B50;
constexpr size_t MaximumPackageBytes = 512ull * 1024 * 1024;
constexpr size_t MaximumEntryBytes = 256ull * 1024 * 1024;
constexpr size_t MaximumEntryCount = 4096;

uint16_t ReadU16(const uint8_t* data)
{
	return static_cast<uint16_t>(static_cast<uint16_t>(data[0]) |
		static_cast<uint16_t>(data[1]) << 8);
}

uint32_t ReadU32(const uint8_t* data)
{
	return static_cast<uint32_t>(data[0]) |
		static_cast<uint32_t>(data[1]) << 8 |
		static_cast<uint32_t>(data[2]) << 16 |
		static_cast<uint32_t>(data[3]) << 24;
}

std::string FileNameOnly(std::string path)
{
	std::replace(path.begin(), path.end(), '\\', '/');
	const size_t separator = path.find_last_of('/');
	if (separator != std::string::npos) path.erase(0, separator + 1);
	return path;
}

class PackageArchive
{
  public:
	bool Load(const void* source, size_t sourceSize)
	{
		if (!source || sourceSize < 22 || sourceSize > MaximumPackageBytes) return false;
		const auto* bytes = static_cast<const uint8_t*>(source);
		const size_t searchBegin = sourceSize > 0xFFFF + 22 ? sourceSize - (0xFFFF + 22) : 0;
		size_t endOffset = sourceSize - 22;
		while (endOffset >= searchBegin && ReadU32(bytes + endOffset) != ZipEndHeader)
		{
			if (endOffset == 0) return false;
			--endOffset;
		}
		if (ReadU32(bytes + endOffset) != ZipEndHeader) return false;
		const uint16_t entryCount = ReadU16(bytes + endOffset + 10);
		const uint32_t centralSize = ReadU32(bytes + endOffset + 12);
		const uint32_t centralOffset = ReadU32(bytes + endOffset + 16);
		if (entryCount == 0 || entryCount > MaximumEntryCount ||
			static_cast<size_t>(centralOffset) + centralSize > sourceSize) return false;

		size_t cursor = centralOffset;
		size_t expandedBytes = 0;
		for (uint16_t i = 0; i < entryCount; ++i)
		{
			if (cursor + 46 > sourceSize || ReadU32(bytes + cursor) != ZipCentralHeader) return false;
			const uint16_t flags = ReadU16(bytes + cursor + 8);
			const uint16_t method = ReadU16(bytes + cursor + 10);
			const uint32_t compressedSize = ReadU32(bytes + cursor + 20);
			const uint32_t uncompressedSize = ReadU32(bytes + cursor + 24);
			const uint16_t nameLength = ReadU16(bytes + cursor + 28);
			const uint16_t extraLength = ReadU16(bytes + cursor + 30);
			const uint16_t commentLength = ReadU16(bytes + cursor + 32);
			const uint32_t localOffset = ReadU32(bytes + cursor + 42);
			const size_t next = cursor + 46ull + nameLength + extraLength + commentLength;
			if (next > sourceSize || localOffset + 30ull > sourceSize ||
				ReadU32(bytes + localOffset) != ZipLocalHeader || flags & 1 ||
				(method != 0 && method != 8) || uncompressedSize > MaximumEntryBytes ||
				expandedBytes + uncompressedSize > MaximumPackageBytes) return false;
			const std::string name(
				reinterpret_cast<const char*>(bytes + cursor + 46), nameLength);
			const uint16_t localNameLength = ReadU16(bytes + localOffset + 26);
			const uint16_t localExtraLength = ReadU16(bytes + localOffset + 28);
			const size_t dataOffset = localOffset + 30ull + localNameLength + localExtraLength;
			if (dataOffset + compressedSize > sourceSize) return false;

			std::vector<uint8_t> output(uncompressedSize);
			if (method == 0)
			{
				if (compressedSize != uncompressedSize) return false;
				if (uncompressedSize > 0)
					std::memcpy(output.data(), bytes + dataOffset, uncompressedSize);
			}
			else if (uncompressedSize > 0)
			{
				const int result = stbi_zlib_decode_noheader_buffer(
					reinterpret_cast<char*>(output.data()), static_cast<int>(output.size()),
					reinterpret_cast<const char*>(bytes + dataOffset), static_cast<int>(compressedSize));
				if (result != static_cast<int>(uncompressedSize)) return false;
			}
			files[FileNameOnly(name)] = std::move(output);
			expandedBytes += uncompressedSize;
			cursor = next;
		}

		const auto metadata = files.find("metafile.json");
		if (metadata == files.end()) return false;
		try
		{
			const std::string text(metadata->second.begin(), metadata->second.end());
			const json root = json::parse(text);
			const auto& entries = root.at("files");
			for (auto it = entries.begin(); it != entries.end(); ++it)
			{
				if (it.value().value("type", std::string{}) != "Effect") continue;
				const auto effect = files.find(FileNameOnly(it.key()));
				if (effect == files.end() || effect->second.empty()) continue;
				effectData = effect->second;
				effectName = it.value().value("relative_path",
					it.value().value("name", std::string("effect.efkefc")));
				break;
			}
		}
		catch (const json::exception&)
		{
			return false;
		}
		return !effectData.empty();
	}

	const std::vector<uint8_t>* Find(const std::string& path) const
	{
		const auto found = files.find(FileNameOnly(path));
		return found == files.end() ? nullptr : &found->second;
	}

	std::vector<uint8_t> effectData;
	std::string effectName;

  private:
	std::unordered_map<std::string, std::vector<uint8_t>> files;
};

class PackageFileReader : public Effekseer::FileReader
{
  public:
	explicit PackageFileReader(const std::vector<uint8_t>* data) : data(data) {}

	size_t Read(void* buffer, size_t size) override
	{
		if (!data || !buffer) return 0;
		const size_t readable = std::min(size, data->size() - position);
		if (readable > 0) std::memcpy(buffer, data->data() + position, readable);
		position += readable;
		return readable;
	}

	void Seek(int value) override
	{
		position = static_cast<size_t>(std::clamp(value, 0, static_cast<int>(data->size())));
	}

	int GetPosition() const override { return static_cast<int>(position); }
	size_t GetLength() const override { return data ? data->size() : 0; }

  private:
	const std::vector<uint8_t>* data = nullptr;
	size_t position = 0;
};

class PackageFileInterface : public Effekseer::FileInterface
{
  public:
	explicit PackageFileInterface(std::shared_ptr<PackageArchive> archive) : archive(std::move(archive)) {}

	Effekseer::FileReaderRef OpenRead(const char16_t* path) override
	{
		if (!path) return nullptr;
		std::string narrow;
		for (; *path; ++path) narrow.push_back(static_cast<char>(*path));
		const auto* data = archive->Find(narrow);
		if (!data) return nullptr;
		return Effekseer::MakeRefPtr<PackageFileReader>(data);
	}

	Effekseer::FileWriterRef OpenWrite(const char16_t*) override { return nullptr; }

  private:
	std::shared_ptr<PackageArchive> archive;
};
}

// コンストラクタ
Effect::Effect(const char* filename)
{
	// エフェクトを読み込みする前にロックする
	// ※マルチスレッドでEffectを作成するとDeviceContextを同時アクセスして
	// 　フリーズする可能性があるので排他制御する
	//std::lock_guard<std::mutex> lock(Game::Graphics::Instance().());

	// Effekseerのリソースを読み込む
	// EffekseerはUTF-16のファイルパス以外は対応していないため文字コード変換が必要
	char16_t utf16Filename[256];
	Effekseer::ConvertUtf8ToUtf16(utf16Filename, 256, filename);

	// Effekseer::Managerを取得
	Effekseer::ManagerRef effekseerManager = EffectManager::Instance().GetEffekseerManager();

	// Effekseerエフェクトを読み込み
	effekseerEffect = Effekseer::Effect::Create(effekseerManager, (EFK_CHAR*)utf16Filename);
}

Effect::Effect(const void* data, size_t size)
{
	if (!data || size == 0 || size > static_cast<size_t>(INT32_MAX)) return;
	Effekseer::ManagerRef manager = EffectManager::Instance().GetEffekseerManager();
	if (size >= 4 && ReadU32(static_cast<const uint8_t*>(data)) == ZipLocalHeader)
	{
		auto package = std::make_shared<PackageArchive>();
		if (!package->Load(data, size)) return;
		auto fileInterface = Effekseer::MakeRefPtr<PackageFileInterface>(package);
		auto renderer = EffectManager::Instance().GetEffekseerRenderer();
		const auto oldTextureLoader = manager->GetTextureLoader();
		const auto oldModelLoader = manager->GetModelLoader();
		const auto oldMaterialLoader = manager->GetMaterialLoader();
		const auto oldCurveLoader = manager->GetCurveLoader();
		manager->SetTextureLoader(renderer->CreateTextureLoader(fileInterface));
		manager->SetModelLoader(renderer->CreateModelLoader(fileInterface));
		manager->SetMaterialLoader(renderer->CreateMaterialLoader(fileInterface));
		manager->SetCurveLoader(Effekseer::MakeRefPtr<Effekseer::CurveLoader>(fileInterface));
		effekseerEffect = Effekseer::Effect::Create(manager, package->effectData.data(),
			static_cast<int32_t>(package->effectData.size()), 1.0f, u"");
		manager->SetTextureLoader(oldTextureLoader);
		manager->SetModelLoader(oldModelLoader);
		manager->SetMaterialLoader(oldMaterialLoader);
		manager->SetCurveLoader(oldCurveLoader);
		return;
	}
	effekseerEffect = Effekseer::Effect::Create(
		manager, data, static_cast<int32_t>(size));
}

bool Effect::IsPackageValid(const void* data, size_t size)
{
	PackageArchive package;
	return package.Load(data, size);
}

// デストラクタ
Effect::~Effect()
{
}

// 再生
Effekseer::Handle Effect::Play(const Vector3& position, float scale)
{
	Effekseer::ManagerRef effekseerManager = EffectManager::Instance().GetEffekseerManager();
	if (!effekseerEffect) return -1;

	Effekseer::Handle handle = effekseerManager->Play(effekseerEffect, position.x, position.y, position.z);
	effekseerManager->SetScale(handle, scale, scale, scale);
	return handle;
}

// 停止
void Effect::Stop(Effekseer::Handle handle)
{
	Effekseer::ManagerRef effekseerManager = EffectManager::Instance().GetEffekseerManager();

	effekseerManager->StopEffect(handle);
}

// 座標設定
void Effect::SetPosition(Effekseer::Handle handle, const Vector3& position)
{
	Effekseer::ManagerRef effekseerManager = EffectManager::Instance().GetEffekseerManager();

	effekseerManager->SetLocation(handle, position.x, position.y, position.z);
}

// スケール設定
void Effect::SetScale(Effekseer::Handle handle, const Vector3& scale)
{
	Effekseer::ManagerRef effekseerManager = EffectManager::Instance().GetEffekseerManager();

	effekseerManager->SetScale(handle, scale.x, scale.y, scale.z);
}

void Effect::SetTransform(Effekseer::Handle handle, const Matrix& transform)
{
	Effekseer::Matrix43 matrix;
	for (int row = 0; row < 4; ++row)
		for (int column = 0; column < 3; ++column)
			matrix.Value[row][column] = transform.m[row][column];
	// ノード追従はエフェクト内部のシミュレーション座標を
	// 書き換えるのではなく、エフェクト全体のベース行列で移動させる。
	EffectManager::Instance().GetEffekseerManager()->SetBaseMatrix(handle, matrix);
}
