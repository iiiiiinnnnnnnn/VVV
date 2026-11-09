#pragma once

#include "Gameplay/Stage/Stage.h"

class StageTitle : public Stage
{
  public:
	void OnAwake() override;

	const Vector3& GetShowcasePosition() const
	{
		return showcasePosition;
	}

  private:
	Vector3 showcasePosition;
};