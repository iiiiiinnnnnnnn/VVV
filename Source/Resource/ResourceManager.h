// ResourceManager.h

#pragma once
#include <memory>
#include <string>
#include <unordered_map>

#include "Core/Foundation/Common.h"
#include "Resource/Model.h"
#include "Resource/Texture.h"

class ResourceManager
{
private:
	ResourceManager() = default;
	~ResourceManager() = default;

public:
	static ResourceManager& Instance()
	{
		static ResourceManager instance;
		return instance;
	}

	std::shared_ptr<Model> LoadModel(const std::string& key)
	{
		const std::string cacheKey = NormalizeKey(key);
		auto it = models.find(cacheKey);
		if (it == models.end())
		{
			auto model = std::make_shared<Model>(cacheKey.c_str());
			it = models.emplace(cacheKey, std::move(model)).first;
		}

		return it->second->Clone();
	}

	std::shared_ptr<Texture> LoadTexture(const std::string& key)
	{
		const std::string cacheKey = NormalizeKey(key);
		auto it = textures.find(cacheKey);
		if (it == textures.end())
		{
			auto texture = std::make_shared<Texture>(cacheKey.c_str());
			it = textures.emplace(cacheKey, std::move(texture)).first;
		}

		return it->second->Clone();
	}

private:
	static std::string NormalizeKey(const std::string& key)
	{
		return std::filesystem::path(key).lexically_normal().generic_string();
	}

	std::unordered_map<std::string, std::shared_ptr<Model>> models;
	std::unordered_map<std::string, std::shared_ptr<Texture>> textures;
};
