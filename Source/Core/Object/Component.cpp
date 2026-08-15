#include "Core/Object/Component.h"

#include "Resource/ResourceManager.h"

#include <utility>

bool Component::SetPreloadResourceFiles(std::vector<std::string> files)
{
	preloadResourceFiles = std::move(files);
	bool succeeded = true;

	// ñ¢ì«çûÇÃÇ›èàóù
	for (const std::string& file : preloadResourceFiles)
	{
		if (file.empty()) continue;
		if (!ResourceManager::Instance().PreloadFile(file)) succeeded = false;
	}
	return succeeded;
}
