#include "Resource/Texture.h"
#include "Resource/ImageData.h"
#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
#include "Resource/CacheBuilder.h"
#endif
#include "Rendering/Core/Graphics.h"
#include "Resource/ResourceManager.h"
#include <stdexcept>

Texture::Texture(ID3D11Device* device, const Color& color)
	: Texture(device,
		(static_cast<UINT>(color.A() * 255) << 24) |
		(static_cast<UINT>(color.B() * 255) << 16) |
		(static_cast<UINT>(color.G() * 255) << 8) |
		static_cast<UINT>(color.R() * 255))
{
}

Texture::Texture(ID3D11Device* device, const char* filename, bool forceRgb)
	: Texture(device, ImageData(filename, forceRgb))
{
}

Texture::Texture(ID3D11Device* device, const uint8_t* data, size_t size)
	: Texture(device, ImageData(data, size))
{
}

Texture::Texture(ID3D11Device* device, const ImageData& image)
{
	if (!device) throw std::invalid_argument("Texture device is null.");
	const auto& pixels = image.GetImage();
	if (FAILED(DirectX::CreateShaderResourceView(device, pixels.GetImages(), pixels.GetImageCount(),
		image.GetMetadata(), shaderResourceView.GetAddressOf())))
		throw std::runtime_error("Texture shader resource view creation failed.");
	Microsoft::WRL::ComPtr<ID3D11Resource> resource;
	shaderResourceView->GetResource(resource.GetAddressOf());
	Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
	if (FAILED(resource.As(&texture))) throw std::runtime_error("Texture is not a 2D texture.");
	texture->GetDesc(&texture2dDesc);
}

Texture::Texture(ID3D11Device* device, UINT color)
	: Texture(device,
		D3D11_TEXTURE2D_DESC{
			1, 1, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, {1, 0},
			D3D11_USAGE_IMMUTABLE, D3D11_BIND_SHADER_RESOURCE, 0, 0},
		D3D11_SUBRESOURCE_DATA{&color, sizeof(color), 0})
{
}

Texture::Texture(ID3D11Device* device, const D3D11_TEXTURE2D_DESC& description, const D3D11_SUBRESOURCE_DATA& data)
	: texture2dDesc(description)
{
	if (!device) throw std::invalid_argument("Texture device is null.");
	Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
	if (FAILED(device->CreateTexture2D(&description, &data, texture.GetAddressOf())) ||
		FAILED(device->CreateShaderResourceView(texture.Get(), nullptr, shaderResourceView.GetAddressOf())))
		throw std::runtime_error("Texture creation failed.");
}

Texture::Texture(ID3D11ShaderResourceView* shaderResourceView, const D3D11_TEXTURE2D_DESC& texture2dDesc)
	: shaderResourceView(shaderResourceView), texture2dDesc(texture2dDesc)
{
}

std::shared_ptr<Texture> Texture::Clone() const
{
	return std::make_shared<Texture>(*this);
}

MipmapTexture::MipmapTexture(const char* filename)
{
	Load(filename);
}

bool MipmapTexture::Load(const char* filename)
{
	auto* device = Game::Graphics::Instance().GetDevice();
	sourceFilePath = filename ? std::filesystem::path(filename).lexically_normal().generic_string() : "";
	loadedFilePath.clear();
	try
	{
		if (!filename) throw std::invalid_argument("Texture filename is null.");
		std::filesystem::path loadPath = ResourceManager::Instance().ResolveResourcePath(
			filename, ResourceManager::ResourceType::Texture);
		if (ToLowerWString(loadPath.extension().wstring()) != L".dds")
		{
			auto ddsPath = loadPath;
			ddsPath.replace_extension(".dds");
			if (!std::filesystem::exists(ddsPath))
			{
#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
				if (FAILED(CacheBuilder::CreateDDSCache(loadPath, ddsPath)))
					throw std::runtime_error("DDS cache creation failed.");
#else
				throw std::runtime_error("DDS cache not found.");
#endif
			}
			loadPath = ddsPath;
		}
		Texture texture(device, loadPath.generic_string().c_str());
		shaderResourceView = texture.GetShaderResourceView();
		texture2dDesc = texture.GetTexture2dDesc();
		loadedFilePath = loadPath.generic_string();
		return true;
	}
	catch (const std::exception&)
	{
		Texture fallback(device, 0xFFFFFFFFu);
		shaderResourceView = fallback.GetShaderResourceView();
		texture2dDesc = fallback.GetTexture2dDesc();
		return false;
	}
}

void Texture::Copy(ID3D11DeviceContext* context, ID3D11Texture2D* destination, ID3D11Texture2D* source)
{
	context->CopyResource(destination, source);
}

void Texture::Update(ID3D11DeviceContext* context, ID3D11Texture2D* texture,
	const void* data, UINT rowPitch, UINT depthPitch)
{
	context->UpdateSubresource(texture, 0, nullptr, data, rowPitch, depthPitch);
}
