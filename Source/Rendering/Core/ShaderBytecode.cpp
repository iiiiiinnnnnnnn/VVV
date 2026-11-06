#include "Rendering/Core/ShaderBytecode.h"
#include "Resource/ResourceManager.h"

#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>

void Gpu::ShaderBytecode::Load(const char* filename)
{
	if (!filename || !*filename) throw std::invalid_argument("Invalid shader filename.");
	const auto& resources = ResourceManager::Instance();
	const auto path = resources.ResolvePath(resources.ResolveResourcePath(filename, ResourceManager::ResourceType::Shader));
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file) throw std::runtime_error("Shader not found: " + std::string(filename));
	const auto size = file.tellg();
	if (size <= 0) throw std::runtime_error("Shader is empty: " + std::string(filename));
	std::vector<uint8_t> loaded(static_cast<size_t>(size));
	file.seekg(0, std::ios::beg);
	if (!file.read(reinterpret_cast<char*>(loaded.data()), static_cast<std::streamsize>(loaded.size())))
		throw std::runtime_error("Could not read shader: " + std::string(filename));
	bytes = std::move(loaded);
}
