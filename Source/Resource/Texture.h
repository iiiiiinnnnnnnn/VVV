#pragma once
#include <d3d11.h>
#include <wrl.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <filesystem>
#include <string>

#include "Core/Foundation/Common.h"

class ImageData;

// テクスチャ
class Texture
{
public:
	// DDS、PNGなど
	Texture(ID3D11Device* device, const char* filename, bool forceRgb = false);
	// メモリから
	Texture(ID3D11Device* device, const uint8_t* data, size_t size);
	// 色で作成
	Texture(ID3D11Device* device, const Color& color);
	Texture(ID3D11Device* device, UINT color);
	// ピクセルデータと作成設定
	Texture(ID3D11Device* device, const D3D11_TEXTURE2D_DESC& description, const D3D11_SUBRESOURCE_DATA& data);
	// 既存のシェーダーリソースビューから作成
	Texture(ID3D11ShaderResourceView* shaderResourceView, const D3D11_TEXTURE2D_DESC& texture2dDesc);
	~Texture() = default;

	std::shared_ptr<Texture> Clone() const;
	static void Copy(ID3D11DeviceContext* context, ID3D11Texture2D* destination, ID3D11Texture2D* source);
	static void Update(ID3D11DeviceContext* context, ID3D11Texture2D* texture,
		const void* data, UINT rowPitch, UINT depthPitch = 0);

	// シェーダーリソースビュー取得
	inline const Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& GetShaderResourceView() const { return shaderResourceView; }

	// テクスチャの情報取得
	inline const D3D11_TEXTURE2D_DESC& GetTexture2dDesc() const { return texture2dDesc; }

	// テクスチャ幅取得
	inline int GetWidth() const { return texture2dDesc.Width; }

	// テクスチャ高さ取得
	inline int GetHeight() const { return texture2dDesc.Height; }

private:
	Texture(ID3D11Device* device, const ImageData& image);
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	shaderResourceView;
	D3D11_TEXTURE2D_DESC	texture2dDesc{};
};

// MipmapつきDDSキャッシュを自動生成して読み込むテクスチャ
class MipmapTexture
{
public:
	MipmapTexture() = default;
	MipmapTexture(const char* filename);

	bool Load(const char* filename);

	inline const Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& GetShaderResourceView() const { return shaderResourceView; }
	inline const D3D11_TEXTURE2D_DESC& GetTexture2dDesc() const { return texture2dDesc; }
	inline const std::string& GetSourceFilePath() const { return sourceFilePath; }
	inline const std::string& GetLoadedFilePath() const { return loadedFilePath; }

private:
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderResourceView;
	D3D11_TEXTURE2D_DESC texture2dDesc{};
	std::string sourceFilePath;
	std::string loadedFilePath;
};
