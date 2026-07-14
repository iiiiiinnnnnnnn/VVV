// Texture.h

#pragma once
#include <d3d11.h>
#include <wrl.h>

#include <filesystem>
#include <string>

#include "Core/Foundation/Common.h"

// テクスチャ
class Texture
{
public:
	// ファイルから
	Texture(const char* filename);
	Texture(const Color& color);
	// シェーダーリソースビューから
	Texture(ID3D11ShaderResourceView* shaderResourceView, const D3D11_TEXTURE2D_DESC& texture2dDesc);
	~Texture() {}

	// シェーダーリソースビュー取得
	inline const Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& GetShaderResourceView() const { return shaderResourceView; }

	// テクスチャの情報取得
	inline const D3D11_TEXTURE2D_DESC& GetTexture2dDesc() const { return texture2dDesc; }

	// テクスチャ幅取得
	inline int GetWidth() const { return texture2dDesc.Width; }

	// テクスチャ高さ取得
	inline int GetHeight() const { return texture2dDesc.Height; }

	// テクスチャ読み込み関数
	static HRESULT LoadTexture(ID3D11Device* device, const char* filename, ID3D11ShaderResourceView** shaderResourceView, D3D11_TEXTURE2D_DESC* texture2dDesc = nullptr);

private:
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	shaderResourceView;
	D3D11_TEXTURE2D_DESC	texture2dDesc;
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

	static HRESULT LoadTexture(
		ID3D11Device* device,
		const char* filename,
		ID3D11ShaderResourceView** shaderResourceView,
		D3D11_TEXTURE2D_DESC* texture2dDesc = nullptr,
		std::string* loadedFilePath = nullptr);

private:
	static std::filesystem::path GetDDSCachePath(const std::filesystem::path& sourcePath);
	static HRESULT CreateDDSCache(const std::filesystem::path& sourcePath, const std::filesystem::path& ddsPath);

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderResourceView;
	D3D11_TEXTURE2D_DESC texture2dDesc{};
	std::string sourceFilePath;
	std::string loadedFilePath;
};
