// Texture.cpp

#include "Resource/Texture.h"

//#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <DirectXTex.h>

#include "Rendering/Core/Graphics.h"
#include "Application/SettingsAndDebug/DebugUtil.h"
#include "Resource/GpuResourceUtils.h"

// テクスチャ読み込み
HRESULT Texture::LoadTexture(
	ID3D11Device* device,
	const char* filename,
	ID3D11ShaderResourceView** shaderResourceView,
	D3D11_TEXTURE2D_DESC* texture2dDesc)
{
	// 拡張子を取得
	std::filesystem::path filepath(filename);
	std::string extension = filepath.extension().string();
	std::transform(extension.begin(), extension.end(), extension.begin(), tolower);	// 小文字化

	// ワイド文字に変換
	std::wstring wfilename = filepath.wstring();
	if(!std::filesystem::exists(wfilename))
	{
		return E_FAIL;
	}

	// フォーマット毎に画像読み込み処理
	HRESULT hr;
	DirectX::TexMetadata metadata;
	DirectX::ScratchImage scratch_image;
	if (extension == ".tga")
	{
		hr = DirectX::GetMetadataFromTGAFile(wfilename.c_str(), metadata);
		if (FAILED(hr)) return hr;

		hr = DirectX::LoadFromTGAFile(wfilename.c_str(), &metadata, scratch_image);
		if (FAILED(hr)) return hr;
	}
	else if (extension == ".dds")
	{
		hr = DirectX::GetMetadataFromDDSFile(wfilename.c_str(), DirectX::DDS_FLAGS_NONE, metadata);
		if (FAILED(hr)) return hr;

		hr = DirectX::LoadFromDDSFile(wfilename.c_str(), DirectX::DDS_FLAGS_NONE, &metadata, scratch_image);
		if (FAILED(hr)) return hr;
	}
	else if (extension == ".hdr")
	{
		hr = DirectX::GetMetadataFromHDRFile(wfilename.c_str(), metadata);
		if (FAILED(hr)) return hr;

		hr = DirectX::LoadFromHDRFile(wfilename.c_str(), &metadata, scratch_image);
		if (FAILED(hr)) return hr;
	}
	else
	{
		hr = DirectX::GetMetadataFromWICFile(wfilename.c_str(), DirectX::WIC_FLAGS_NONE, metadata);
		if (FAILED(hr)) return hr;

		hr = DirectX::LoadFromWICFile(wfilename.c_str(), DirectX::WIC_FLAGS_NONE, &metadata, scratch_image);
		if (FAILED(hr)) return hr;
	}

	// シェーダーリソースビュー作成
	hr = DirectX::CreateShaderResourceView(device, scratch_image.GetImages(), scratch_image.GetImageCount(),
		metadata, shaderResourceView);
	_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

	// テクスチャ情報取得
	if (texture2dDesc != nullptr)
	{
		Microsoft::WRL::ComPtr<ID3D11Resource> resource;
		(*shaderResourceView)->GetResource(resource.GetAddressOf());

		Microsoft::WRL::ComPtr<ID3D11Texture2D> texture2d;
		hr = resource->QueryInterface<ID3D11Texture2D>(texture2d.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

		texture2d->GetDesc(texture2dDesc);
	}
	return hr;
}

Texture::Texture(const char* filename)
{
	ID3D11Device* device = Game::Graphics::Instance().GetDevice();

	if (std::filesystem::exists(filename))
	{
		// 拡張子を取得
		std::filesystem::path filepath(filename);
		std::string extension = filepath.extension().string();
		std::transform(extension.begin(), extension.end(), extension.begin(), tolower);	// 小文字化

		// ワイド文字に変換
		std::wstring wfilename = filepath.wstring();

		// フォーマット毎に画像読み込み処理
		HRESULT hr = LoadTexture(device, filename, &shaderResourceView, &texture2dDesc);
		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
	}
	else
	{
		// ダミーテクスチャ
		GpuResourceUtils::CreateDummyTexture(
			device,
			0xFFFFFFFF,
			shaderResourceView.GetAddressOf(),
			&texture2dDesc);
	}
}

Texture::Texture(const Color& color)
{
	UINT packedColor =
		(static_cast<UINT>(color.A() * 255) << 24) |
		(static_cast<UINT>(color.B() * 255) << 16) |
		(static_cast<UINT>(color.G() * 255) << 8)  |
		(static_cast<UINT>(color.R() * 255));

	auto device = Game::Graphics::Instance().GetDevice();
	HRESULT hr = GpuResourceUtils::CreateDummyTexture(
		device,
		packedColor,
		shaderResourceView.GetAddressOf(),
		&texture2dDesc);
	_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
}

Texture::Texture(ID3D11ShaderResourceView* shaderResourceView, const D3D11_TEXTURE2D_DESC& texture2dDesc)
	: shaderResourceView(shaderResourceView), texture2dDesc(texture2dDesc)
{
}

MipmapTexture::MipmapTexture(const char* filename)
{
	Load(filename);
}

bool MipmapTexture::Load(const char* filename)
{
	ID3D11Device* device = Game::Graphics::Instance().GetDevice();

	std::string loadedPath;
	HRESULT hr = LoadTexture(
		device,
		filename,
		shaderResourceView.ReleaseAndGetAddressOf(),
		&texture2dDesc,
		&loadedPath);

	sourceFilePath = filename != nullptr ? std::filesystem::path(filename).lexically_normal().generic_string() : "";
	loadedFilePath = loadedPath;

	if (FAILED(hr))
	{
		GpuResourceUtils::CreateDummyTexture(
			device,
			0xFFFFFFFF,
			shaderResourceView.ReleaseAndGetAddressOf(),
			&texture2dDesc);
		return false;
	}

	return true;
}

HRESULT MipmapTexture::LoadTexture(
	ID3D11Device* device,
	const char* filename,
	ID3D11ShaderResourceView** shaderResourceView,
	D3D11_TEXTURE2D_DESC* texture2dDesc,
	std::string* loadedFilePath)
{
	if (device == nullptr || filename == nullptr || shaderResourceView == nullptr)
	{
		return E_INVALIDARG;
	}

	const std::filesystem::path sourcePath = std::filesystem::path(filename).lexically_normal();
	const std::wstring extension = ToLowerWString(sourcePath.extension().wstring());

	std::filesystem::path loadPath = sourcePath;
	if (extension != L".dds")
	{
		const std::filesystem::path ddsPath = GetDDSCachePath(sourcePath);
		if (!std::filesystem::exists(ddsPath))
		{
			HRESULT hr = CreateDDSCache(sourcePath, ddsPath);
			if (FAILED(hr))
			{
				return hr;
			}
		}

		loadPath = ddsPath;
	}

	if (!std::filesystem::exists(loadPath))
	{
		return E_FAIL;
	}

	HRESULT hr = Texture::LoadTexture(
		device,
		loadPath.generic_string().c_str(),
		shaderResourceView,
		texture2dDesc);

	if (SUCCEEDED(hr) && loadedFilePath != nullptr)
	{
		*loadedFilePath = loadPath.generic_string();
	}

	return hr;
}

std::filesystem::path MipmapTexture::GetDDSCachePath(const std::filesystem::path& sourcePath)
{
	std::filesystem::path ddsPath = sourcePath;
	ddsPath.replace_extension(".dds");
	return ddsPath.lexically_normal();
}

HRESULT MipmapTexture::CreateDDSCache(const std::filesystem::path& sourcePath, const std::filesystem::path& ddsPath)
{
	if (!std::filesystem::exists(sourcePath))
	{
		return E_FAIL;
	}

	DirectX::TexMetadata metadata{};
	DirectX::ScratchImage sourceImage;
	HRESULT hr = GpuResourceUtils::LoadImageFile(sourcePath, metadata, sourceImage);
	if (FAILED(hr))
	{
		return hr;
	}

	DirectX::ScratchImage mipImage;
	hr = DirectX::GenerateMipMaps(
		sourceImage.GetImages(),
		sourceImage.GetImageCount(),
		metadata,
		DirectX::TEX_FILTER_DEFAULT,
		0,
		mipImage);

	const DirectX::ScratchImage* saveImage = &mipImage;
	if (FAILED(hr))
	{
		saveImage = &sourceImage;
	}

	std::error_code error;
	if (!ddsPath.parent_path().empty())
	{
		std::filesystem::create_directories(ddsPath.parent_path(), error);
		if (error)
		{
			return E_FAIL;
		}
	}

	return DirectX::SaveToDDSFile(
		saveImage->GetImages(),
		saveImage->GetImageCount(),
		saveImage->GetMetadata(),
		DirectX::DDS_FLAGS_NONE,
		ddsPath.wstring().c_str());
}
