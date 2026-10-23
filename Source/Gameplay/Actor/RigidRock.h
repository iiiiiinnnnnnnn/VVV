#include "Gameplay/Actor/Actor.h"
#include "Rendering/Component/VMDL.h"

#pragma once

class RigidRock : public Actor
{
  public:
	RigidRock(const Transform& transform = {}, const std::string& modelPath = "RigidRock");
	~RigidRock() override;

  protected:
	void OnInit() override;
	void OnUninit() override;

	VMDL* vmdl = nullptr;
	std::string modelPath;
};