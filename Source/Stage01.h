// Stage01.h

#pragma once

#include "Actor.h"

class Stage01 : public Actor
{
public:
	Stage01(ActorManager* actorManager);
	void ApplyEnvironment(class LightManager& lightManager) const;
	void OnUpdate() override;
	void OnDrawGUI() override;

private:

};
