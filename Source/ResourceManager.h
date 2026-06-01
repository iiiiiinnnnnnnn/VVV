// ResourceManager.h

#pragma once

#include "Common.h"
#include "Model.h"
#include "Texture.h"

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
		return std::make_shared<Model>(key.c_str());
	}

	std::shared_ptr<Texture> LoadTexture(const std::string& key)
	{
		return std::make_shared<Texture>(key.c_str());
	}
};