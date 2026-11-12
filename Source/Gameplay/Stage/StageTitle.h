#pragma once

#include "Gameplay/Stage/Stage.h"

class StageTitle : public Stage
{
  public:
	void OnAwake() override;

	bool ShouldLoadStageProp(const std::string& path) const override;
};
