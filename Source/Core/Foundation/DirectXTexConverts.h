// DirectXTexConverts.cpp

#pragma once

#include "Common.h"
#include <DirectXTex.h>
#include <wrl.h>
#include <vector>
#include <filesystem>

bool SaveDDSAsPNG(
	const std::vector<uint8_t>& ddsData,
	const std::filesystem::path& outputPath)
{
	if (ddsData.empty())
	{
		return false;
	}

	DirectX::ScratchImage loadedImage;
	DirectX::TexMetadata metadata{};

	HRESULT hr = DirectX::LoadFromDDSMemory(
		ddsData.data(),
		ddsData.size(),
		DirectX::DDS_FLAGS_NONE,
		&metadata,
		loadedImage);

	if (FAILED(hr))
	{
		return false;
	}

	const DirectX::Image* sourceImage = loadedImage.GetImage(0, 0, 0);
	if (sourceImage == nullptr)
	{
		return false;
	}

	DirectX::ScratchImage decompressedImage;
	DirectX::ScratchImage convertedImage;

	if (DirectX::IsCompressed(sourceImage->format))
	{
		hr = DirectX::Decompress(
			*sourceImage,
			DXGI_FORMAT_R8G8B8A8_UNORM,
			decompressedImage);

		if (FAILED(hr))
		{
			return false;
		}

		sourceImage = decompressedImage.GetImage(0, 0, 0);
		if (sourceImage == nullptr)
		{
			return false;
		}
	}
	else if (sourceImage->format != DXGI_FORMAT_R8G8B8A8_UNORM &&
		sourceImage->format != DXGI_FORMAT_B8G8R8A8_UNORM &&
		sourceImage->format != DXGI_FORMAT_B8G8R8X8_UNORM)
	{
		hr = DirectX::Convert(
			*sourceImage,
			DXGI_FORMAT_R8G8B8A8_UNORM,
			DirectX::TEX_FILTER_DEFAULT,
			DirectX::TEX_THRESHOLD_DEFAULT,
			convertedImage);

		if (FAILED(hr))
		{
			return false;
		}

		sourceImage = convertedImage.GetImage(0, 0, 0);
		if (sourceImage == nullptr)
		{
			return false;
		}
	}

	hr = DirectX::SaveToWICFile(
		*sourceImage,
		DirectX::WIC_FLAGS_NONE,
		DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG),
		outputPath.c_str());

	return SUCCEEDED(hr);
}


HRESULT SaveScratchImageToDDSBytes(
	const DirectX::ScratchImage& sourceImage,
	std::vector<uint8_t>& outDDS)
{
	outDDS.clear();

	constexpr size_t MaxEmbeddedTextureSize = 2048;
	const DirectX::TexMetadata& sourceMetadata = sourceImage.GetMetadata();

	// DDSへ統一する前にRGBA8へ変換する。埋め込みサイズを抑えるため長辺を2048pxに制限し、
	// 縮小後の画像からミップを生成する。ミップ生成に失敗しても元画像は保存できる。
	DirectX::ScratchImage rgbaImage;
	HRESULT hr = S_OK;

	if (sourceMetadata.format != DXGI_FORMAT_R8G8B8A8_UNORM)
	{
		hr = DirectX::Convert(
			sourceImage.GetImages(),
			sourceImage.GetImageCount(),
			sourceMetadata,
			DXGI_FORMAT_R8G8B8A8_UNORM,
			DirectX::TEX_FILTER_DEFAULT,
			DirectX::TEX_THRESHOLD_DEFAULT,
			rgbaImage
		);

		if (FAILED(hr))
		{
			return hr;
		}
	}
	else
	{
		hr = rgbaImage.InitializeFromImage(*sourceImage.GetImage(0, 0, 0));

		if (FAILED(hr))
		{
			return hr;
		}
	}

	const DirectX::ScratchImage* mipSource = &rgbaImage;

	DirectX::ScratchImage resizedImage;
	const DirectX::TexMetadata& rgbaMetadata = rgbaImage.GetMetadata();
	const size_t maxSize = (std::max)(rgbaMetadata.width, rgbaMetadata.height);
	if (maxSize > MaxEmbeddedTextureSize)
	{
		const double scale = static_cast<double>(MaxEmbeddedTextureSize) / static_cast<double>(maxSize);
		const size_t resizedWidth = (std::max)(static_cast<size_t>(1), static_cast<size_t>(rgbaMetadata.width * scale));
		const size_t resizedHeight = (std::max)(static_cast<size_t>(1), static_cast<size_t>(rgbaMetadata.height * scale));

		hr = DirectX::Resize(
			rgbaImage.GetImages(),
			rgbaImage.GetImageCount(),
			rgbaMetadata,
			resizedWidth,
			resizedHeight,
			DirectX::TEX_FILTER_DEFAULT,
			resizedImage
		);

		if (FAILED(hr))
		{
			return hr;
		}

		mipSource = &resizedImage;
	}

	const DirectX::ScratchImage* saveSource = mipSource;

	DirectX::ScratchImage mipImage;
	hr = DirectX::GenerateMipMaps(
		mipSource->GetImages(),
		mipSource->GetImageCount(),
		mipSource->GetMetadata(),
		DirectX::TEX_FILTER_DEFAULT,
		0,
		mipImage
	);

	if (SUCCEEDED(hr))
	{
		saveSource = &mipImage;
	}

	DirectX::Blob blob;
	hr = DirectX::SaveToDDSMemory(
		saveSource->GetImages(),
		saveSource->GetImageCount(),
		saveSource->GetMetadata(),
		DirectX::DDS_FLAGS_NONE,
		blob
	);

	if (FAILED(hr))
	{
		return hr;
	}

	outDDS.resize(blob.GetBufferSize());
	memcpy(outDDS.data(), blob.GetBufferPointer(), blob.GetBufferSize());

	return S_OK;
}

HRESULT ConvertTextureFileToDDSBytes(
	const std::filesystem::path& texturePath,
	std::vector<uint8_t>& outDDS)
{
	outDDS.clear();

	if (!std::filesystem::exists(texturePath))
	{
		return E_FAIL;
	}

	std::wstring extension = ToLowerWString(texturePath.extension().wstring());

	if (extension == L".dds")
	{
		return ReadBinaryFile(texturePath, outDDS) ? S_OK : E_FAIL;
	}

	DirectX::ScratchImage image;
	DirectX::TexMetadata metadata = {};
	HRESULT hr = E_FAIL;

	if (extension == L".tga")
	{
		hr = DirectX::LoadFromTGAFile(
			texturePath.wstring().c_str(),
			&metadata,
			image
		);
	}
	else
	{
		hr = DirectX::LoadFromWICFile(
			texturePath.wstring().c_str(),
			DirectX::WIC_FLAGS_FORCE_RGB,
			&metadata,
			image
		);
	}

	if (FAILED(hr))
	{
		return hr;
	}

	return SaveScratchImageToDDSBytes(image, outDDS);
}

HRESULT ConvertSRVToDDSBytes(
	ID3D11Device* device,
	ID3D11ShaderResourceView* srv,
	std::vector<uint8_t>& outDDS)
{
	outDDS.clear();

	if (device == nullptr || srv == nullptr)
	{
		return E_FAIL;
	}

	Microsoft::WRL::ComPtr<ID3D11Resource> resource;
	srv->GetResource(resource.GetAddressOf());

	if (resource == nullptr)
	{
		return E_FAIL;
	}

	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
	device->GetImmediateContext(context.GetAddressOf());

	if (context == nullptr)
	{
		return E_FAIL;
	}

	DirectX::ScratchImage image;
	HRESULT hr = DirectX::CaptureTexture(
		device,
		context.Get(),
		resource.Get(),
		image
	);

	if (FAILED(hr))
	{
		return hr;
	}

	return SaveScratchImageToDDSBytes(image, outDDS);
}
