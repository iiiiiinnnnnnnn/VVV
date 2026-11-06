#include "Resource/ImageData.h"
#include "Resource/ResourceManager.h"
#include "Core/Foundation/Common.h"
#include <stdexcept>

ImageData::ImageData(const char* filename, bool forceRgb)
{
	if (!filename) throw std::invalid_argument("Texture filename is null.");
	if (FAILED(LoadFile(filename, forceRgb)))
		throw std::runtime_error(std::string("Texture load failed: ") + filename);
}

ImageData::ImageData(const uint8_t* data, size_t size)
{
	if (FAILED(LoadMemory(data, size))) throw std::runtime_error("Texture memory load failed.");
}

HRESULT ImageData::PrepareImageDecoder()
{
	struct ComScope
	{
		HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		~ComScope() { if (SUCCEEDED(result)) CoUninitialize(); }
	};
	thread_local const ComScope com;
	return com.result == RPC_E_CHANGED_MODE ? S_OK : com.result;
}

HRESULT ImageData::LoadFile(const std::filesystem::path& filepath, bool forceRgb)
{
	const HRESULT decoder = PrepareImageDecoder();
	if (FAILED(decoder)) return decoder;
	const auto& resources = ResourceManager::Instance();
	const std::filesystem::path resolved = resources.ResolveResourcePath(
		filepath.generic_string(), ResourceManager::ResourceType::Texture);
	const std::wstring extension = ToLowerWString(resolved.extension().wstring());
	const std::wstring filename = resolved.wstring();

	if (extension == L".dds")
	{
		return DirectX::LoadFromDDSFile(
			filename.c_str(),
			DirectX::DDS_FLAGS_NONE,
			&metadata,
			image);
	}

	if (extension == L".tga")
	{
		return DirectX::LoadFromTGAFile(
			filename.c_str(),
			&metadata,
			image);
	}

	if (extension == L".hdr")
	{
		return DirectX::LoadFromHDRFile(
			filename.c_str(),
			&metadata,
			image);
	}

	return DirectX::LoadFromWICFile(
		filename.c_str(),
		forceRgb ? DirectX::WIC_FLAGS_FORCE_RGB : DirectX::WIC_FLAGS_NONE,
		&metadata,
		image);
}

HRESULT ImageData::LoadMemory(const uint8_t* data, size_t size)
{
	if (!data || !size) return E_INVALIDARG;
	const HRESULT decoder = PrepareImageDecoder();
	if (FAILED(decoder)) return decoder;
	// フォーマット毎に画像読み込み処理
	HRESULT hr = E_FAIL;

	// .tga
	{
		hr = DirectX::GetMetadataFromTGAMemory(data, size, metadata);
		if (SUCCEEDED(hr))
		{
			hr = DirectX::LoadFromTGAMemory(data, size, &metadata, image);
		}
	}
	// .dds
	if (FAILED(hr))
	{
		hr = DirectX::GetMetadataFromDDSMemory(data, size, DirectX::DDS_FLAGS_NONE, metadata);
		if (SUCCEEDED(hr))
		{
			hr = DirectX::LoadFromDDSMemory(data, size, DirectX::DDS_FLAGS_NONE, &metadata, image);
		}
	}
	// .hdr
	if (FAILED(hr))
	{
		hr = DirectX::GetMetadataFromHDRMemory(data, size, metadata);
		if (SUCCEEDED(hr))
		{
			hr = DirectX::LoadFromHDRMemory(data, size, &metadata, image);
		}
	}
	if (FAILED(hr))
	{
		hr = DirectX::GetMetadataFromWICMemory(data, size, DirectX::WIC_FLAGS_NONE, metadata);
		if (SUCCEEDED(hr))
		{
			hr = DirectX::LoadFromWICMemory(data, size, DirectX::WIC_FLAGS_NONE, &metadata, image);
		}
	}
	if (FAILED(hr))
	{
		return hr;
	}

	return hr;
}
