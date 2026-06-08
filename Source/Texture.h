// Texture.h

#pragma once

#include "Common.h"

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
