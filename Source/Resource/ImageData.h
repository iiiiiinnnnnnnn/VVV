#pragma once
#include <DirectXTex.h>
#include <filesystem>
#include <cstdint>

// CPU側で保持・加工する画像データ
class ImageData
{
public:
	ImageData() = default;
	ImageData(const char* filename, bool forceRgb = false);
	ImageData(const uint8_t* data, size_t size);
	HRESULT LoadFile(const std::filesystem::path& filepath, bool forceRgb = false);
	HRESULT LoadMemory(const uint8_t* data, size_t size);
	const DirectX::TexMetadata& GetMetadata() const { return metadata; }
	const DirectX::ScratchImage& GetImage() const { return image; }

private:
	static HRESULT PrepareImageDecoder();
	DirectX::TexMetadata metadata{};
	DirectX::ScratchImage image;
};
