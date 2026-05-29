// Texture.cpp

#include "Texture.h"

//#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <DirectXTex.h>

#include "Graphics.h"
#include "Misc.h"

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
	ID3D11Device* device = Graphics::Instance().GetDevice();

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

Texture::Texture(ID3D11ShaderResourceView* shaderResourceView, const D3D11_TEXTURE2D_DESC& texture2dDesc)
	: shaderResourceView(shaderResourceView), texture2dDesc(texture2dDesc)
{
}