#pragma once
#include <cstddef>

#include <d3d11.h>
#include <wrl.h>

#include <filesystem>
#include <vector>

#include "Core/Foundation/Common.h"
#include "DirectXTex.h"

// GPUリソースユーティリティ
class GpuResourceUtils
{
public:
	// 頂点シェーダー読み込み
	static HRESULT LoadVertexShader(
		ID3D11Device* device,
		const char* filename,
		const D3D11_INPUT_ELEMENT_DESC inputElementDescs[],
		UINT inputElementCount,
		ID3D11InputLayout** inputLayout,
		ID3D11VertexShader** vertexShader);

	// ピクセルシェーダー読み込み
	static HRESULT LoadPixelShader(
		ID3D11Device* device,
		const char* filename,
		ID3D11PixelShader** pixelShader);

	// ジオメトリシェーダー読み込み
	static HRESULT LoadGeometryShader(
		ID3D11Device* device,
		const char* filename,
		ID3D11GeometryShader** geometryShader);

	// コンピュートシェーダー読み込み
	static HRESULT LoadComputeShader(
		ID3D11Device* device,
		const char* filename,
		ID3D11ComputeShader** computeShader);

	// テクスチャ読み込み
	static HRESULT LoadTexture(
		ID3D11Device* device,
		const char* filename,
		ID3D11ShaderResourceView** shaderResourceView,
		D3D11_TEXTURE2D_DESC* texture2dDesc = nullptr);

	// テクスチャ読み込み
	static HRESULT LoadTexture(
		ID3D11Device* device,
		const uint8_t* data,
		size_t size,
		ID3D11ShaderResourceView** shaderResourceView,
		D3D11_TEXTURE2D_DESC* texture2dDesc = nullptr);

	// ダミーテクスチャ作成
	static HRESULT CreateDummyTexture(
		ID3D11Device* device,
		UINT color,
		ID3D11ShaderResourceView** shaderResourceView,
		D3D11_TEXTURE2D_DESC* texture2dDesc = nullptr);

	// 定数バッファ作成
	static HRESULT CreateConstantBuffer(
		ID3D11Device* device,
		UINT bufferSize,
		ID3D11Buffer** constantBuffer);

	// バイナリファイル読み込み
	static std::vector<uint8_t> LoadBinaryFile(
		const char* filename);

	//ハル シェーダー読み込み
	static void LoadHullShader(
		ID3D11Device* device,
		const char* filename,
		ID3D11HullShader** shader);

	// ドメインシェーダー読み込み
	static void LoadDomainShader(
		ID3D11Device* device,
		const char* filename,
		ID3D11DomainShader** shader);

	// 画像ファイル読み込み
	static HRESULT LoadImageFile(
		const std::filesystem::path& filepath,
		DirectX::TexMetadata& metadata,
		DirectX::ScratchImage& image);
};
