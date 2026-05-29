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

	std::unordered_map<std::string, std::shared_ptr<Model>> modelCache;
	std::unordered_map<std::string, std::shared_ptr<Texture>> textureCache;

public:
	static ResourceManager& Instance() {
		static ResourceManager instance;
		return instance;
	}

	std::shared_ptr<Model> LoadModel(const std::string& key) {
		if (!modelCache.contains(key)) {
			modelCache[key] = std::make_shared<Model>(key.c_str());
		}
		return modelCache[key];
	}

	std::shared_ptr<Texture> LoadTexture(const std::string& key) {
		if (!textureCache.contains(key)) {
			textureCache[key] = std::make_shared<Texture>(key.c_str());
		}
		return textureCache[key];
	}
};