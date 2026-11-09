#include "StageTitle.h"
#include "Component/StageLoader.h"

void StageTitle::OnAwake()
{
	if (!LoadVSTG("cave_02"))
		throw std::runtime_error("Failed to load stage");

	StageLoader* loader = GetComponent<StageLoader>();
	showcasePosition = loader && loader->HasPlayerStart()
										 ? loader->GetPlayerStartTransforms().front().position
										 : Vector3::Zero;
}
